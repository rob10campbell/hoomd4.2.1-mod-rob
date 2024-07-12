// Copyright (c) 2009-2023 The Regents of the University of Michigan.
// Part of HOOMD-blue, released under the BSD 3-Clause License.

// ########## Modified by Rheoinformatic //~ [RHEOINF] ##########

#ifndef __PAIR_EVALUATOR_EWALD_H__
#define __PAIR_EVALUATOR_EWALD_H__

#ifndef __HIPCC__
#include <string>
#endif

#include "hoomd/HOOMDMath.h"
#include "hoomd/VectorMath.h" // add vectors for optional position [RHEOINF]

/*! \file EvaluatorPairEwald.h
    \brief Defines the pair evaluator class for Ewald potentials
*/

// need to declare these class methods with __device__ qualifiers when building in nvcc
// DEVICE is __host__ __device__ when included in nvcc and blank when included into the host
// compiler
#ifdef __HIPCC__
#define DEVICE __device__
#define HOSTDEVICE __host__ __device__
#else
#define DEVICE
#define HOSTDEVICE
#endif

namespace hoomd
    {
namespace md
    {
//! Class for evaluating the Ewald pair potential
/*! <b>General Overview</b>

    See EvaluatorPairLJ

    <b>Ewald specifics</b>

    EvaluatorPairEwald evaluates the function:

    \f[
    V_{\mathrm{ewald}}(r)  = q_i q_j \left[\mathrm{erfc}\left(\kappa r +
   \frac{\alpha}{2\kappa}\right) \exp(\alpha r)+ \mathrm{erfc}\left(\kappa r - \frac{\alpha}{2
   \kappa}\right) \exp(-\alpha r)\right] \f]
*/
class EvaluatorPairEwald
    {
    public:
    //! Define the parameter type used by this pair potential evaluator
    struct param_type
        {
        Scalar kappa;
        Scalar alpha;

        DEVICE void load_shared(char*& ptr, unsigned int& available_bytes) { }

        HOSTDEVICE void allocate_shared(char*& ptr, unsigned int& available_bytes) const { }

#ifdef ENABLE_HIP
        //! Set CUDA memory hints
        void set_memory_hints() const { }
#endif

#ifndef __HIPCC__
        param_type() : kappa(0), alpha(0) { }

        param_type(pybind11::dict v, bool managed = false)
            {
            kappa = v["kappa"].cast<Scalar>();
            alpha = v["alpha"].cast<Scalar>();
            }

        pybind11::dict asDict()
            {
            pybind11::dict v;
            v["kappa"] = kappa;
            v["alpha"] = alpha;
            return v;
            }
#endif
        }
#if HOOMD_LONGREAL_SIZE == 32
        __attribute__((aligned(8)));
#else
        __attribute__((aligned(16)));
#endif

    //! Constructs the pair potential evaluator
    /*! \param _rsq Squared distance between the particles
        \param _radcontact the sum of the interacting particle radii [RHEOINF]
        \param _pair_typeids the typeIDs of the interacting particles [RHEOINF] 
        \param _rcutsq Squared distance at which the potential goes to 0
        \param _params Per type pair parameters of this potential
    */
    DEVICE EvaluatorPairEwald(Scalar _rsq, Scalar _radcontact, unsigned int _pair_typeids[2], Scalar _rcutsq, const param_type& _params) //~ add radcontact, pair_typeiDs [RHEOINF]
        : rsq(_rsq), radcontact(_radcontact), rcutsq(_rcutsq), kappa(_params.kappa), alpha(_params.alpha) //~ add radcontact [RHEOINF]
        {
        typei = _pair_typeids[0]; //~ add typei [RHEOINF]
        typej = _pair_typeids[1]; //~ add typej [RHEOINF] 
        }

    //~ set tags [RHEOINF]
    HOSTDEVICE static bool needsTags()
        {
        return false;
        }
    HOSTDEVICE void setTags(unsigned int tagi, unsigned int tagj) { }
    //~
        
    //!~ add diameter [RHEOINF] 
    DEVICE static bool needsDiameter()
        {
        return false;
        }
    //! Accept the optional diameter values
    /*! \param di Diameter of particle i
        \param dj Diameter of particle j
    */
    DEVICE void setDiameter(Scalar di, Scalar dj) { }
    //~

    //! Ewald uses charge !!!
    DEVICE static bool needsCharge()
        {
        return true;
        }
    //! Accept the optional charge values.
    /*! \param qi Charge of particle i
        \param qj Charge of particle j
    */
    DEVICE void setCharge(Scalar qi, Scalar qj)
        {
        qiqj = qi * qj;
        }

    //~ add timestep [RHEOINF]
    HOSTDEVICE static bool needsTimestep()
        {
        return false;
        }
    HOSTDEVICE void setTimestep(uint64_t timestep)  { }
    //~

    //~ add i and j positions [RHEOINF] 
    DEVICE static bool needsIJPos()
        {
        return false;
        }
    //! Accept the optional position values
    /*! \param pi position of particle i
        \param pj position of particle j
    */
    DEVICE void setIJPos(Scalar3 pi, Scalar3 pj) {}
    //~

    //!~ Whether the potential pair needs BoxDim info [RHEOINF]
    HOSTDEVICE static bool needsBox()
        {
        return false;
        }
    //! Accept the optional BoxDim structure
    /*! \param box the current box
    */
    HOSTDEVICE void setBox(const BoxDim box) { }
    //~

    //! Evaluate the force and energy
    /*! \param force_divr Output parameter to write the computed force divided by r.
        \param pair_eng Output parameter to write the computed pair energy
        \param energy_shift If true, the potential must be shifted so that V(r) is continuous at the
       cutoff \note There is no need to check if rsq < rcutsq in this method. Cutoff tests are
       performed in PotentialPair.

        \return True if they are evaluated or false if they are not because we are beyond the cutoff
    */
    DEVICE bool evalForceAndEnergy(Scalar& force_divr, Scalar& pair_eng, bool energy_shift)
        {
        if (rsq < rcutsq && qiqj != 0)
            {
            Scalar rinv = fast::rsqrt(rsq);
            Scalar r = Scalar(1.0) / rinv;
            Scalar r2inv = Scalar(1.0) / rsq;

            Scalar arg1 = kappa * r + alpha / (Scalar(2.0) * kappa);
            Scalar arg2 = kappa * r - alpha / (Scalar(2.0) * kappa);
            Scalar expfac1 = fast::exp(alpha * r);
            Scalar expfac2 = fast::exp(-alpha * r);
            Scalar val
                = Scalar(0.5) * (fast::erfc(arg1) * expfac1 + fast::erfc(arg2) * expfac2) * rinv;

            force_divr = qiqj * r2inv
                         * (val
                            + expfac2 * Scalar(2.0) * kappa * fast::exp(-arg2 * arg2)
                                  / fast::sqrt(Scalar(M_PI))
                            + alpha * Scalar(0.5) * expfac2 * fast::erfc(arg2)
                            - alpha * Scalar(0.5) * expfac1 * fast::erfc(arg1));
            pair_eng = qiqj * val;

            return true;
            }
        else
            return false;
        }

    DEVICE Scalar evalPressureLRCIntegral()
        {
        return 0;
        }

    DEVICE Scalar evalEnergyLRCIntegral()
        {
        return 0;
        }

#ifndef __HIPCC__
    //! Get the name of this potential
    /*! \returns The potential name.
     */
    static std::string getName()
        {
        return std::string("ewald");
        }

    std::string getShapeSpec() const
        {
        throw std::runtime_error("Shape definition not supported for this pair potential.");
        }
#endif

    protected:
    Scalar rsq;    //!< Stored rsq from the constructor
    Scalar radcontact;//!< Stored contact-distance from the constructor [RHEOINF]
    unsigned int pair_typeids;//!< Stored pair typeIDs from the constructor [RHEOINF]
    unsigned int typei;//!<~ Stored typeID of particle i from the constructor [RHEOINF]
    unsigned int typej;//!<~ Stored typeID of particle j from the constructor [RHEOINF]
    Scalar rcutsq; //!< Stored rcutsq from the constructor
    Scalar kappa;  //!< Splitting parameter
    Scalar alpha;  //!< Debye screening parameter
    Scalar qiqj;   //!< product of qi and qj
    };

    } // end namespace md
    } // end namespace hoomd

#endif // __PAIR_EVALUATOR_EWALD_H__
