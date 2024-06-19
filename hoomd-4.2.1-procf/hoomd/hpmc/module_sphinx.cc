// Copyright (c) 2009-2023 The Regents of the University of Michigan.
// Part of HOOMD-blue, released under the BSD 3-Clause License.

// ########## Modified by Rheoinformatic //~ [RHEOINF] ##########

// Include the defined classes that are to be exported to python
#include "ComputeFreeVolume.h"
#include "IntegratorHPMC.h"
#include "IntegratorHPMCMono.h"
#include "../Variant.h" //~ add vinf [RHEOINF]

#include "ComputeSDF.h"
#include "ShapeSphinx.h"
#include "ShapeUnion.h"

#include "ExternalField.h"
#include "ExternalFieldHarmonic.h"
#include "ExternalFieldWall.h"

#include "UpdaterClusters.h"
#include "UpdaterMuVT.h"

#ifdef ENABLE_HIP
#include "ComputeFreeVolumeGPU.h"
#include "IntegratorHPMCMonoGPU.h"
#include "UpdaterClustersGPU.h"
#endif

namespace hoomd
    {
namespace hpmc
    {
namespace detail
    {
//! Export the base HPMCMono integrators
void export_sphinx(pybind11::module& m)
    {
    //~ Update the function calls to pass both required arguments [RHEOINF]
    m.def("create_IntegratorHPMCMonoSphinx", [](std::shared_ptr<SystemDefinition> sysdef, std::shared_ptr<Variant> vinf)
    {
        return std::make_shared<IntegratorHPMCMono<ShapeSphinx>>(sysdef, vinf);
    });
    //export_IntegratorHPMCMono<ShapeSphinx>(m, "IntegratorHPMCMonoSphinx");
    //~
    export_ComputeFreeVolume<ShapeSphinx>(m, "ComputeFreeVolumeSphinx");
    export_ComputeSDF<ShapeSphinx>(m, "ComputeSDFSphinx");
    export_UpdaterMuVT<ShapeSphinx>(m, "UpdaterMuVTSphinx");
    export_UpdaterClusters<ShapeSphinx>(m, "UpdaterClustersSphinx");

    export_ExternalFieldInterface<ShapeSphinx>(m, "ExternalFieldSphinx");
    export_HarmonicField<ShapeSphinx>(m, "ExternalFieldHarmonicSphinx");
    export_ExternalFieldWall<ShapeSphinx>(m, "WallSphinx");

#ifdef ENABLE_HIP
#ifdef ENABLE_SPHINX_GPU

    export_IntegratorHPMCMonoGPU<ShapeSphinx>(m, "IntegratorHPMCMonoSphinxGPU");
    export_ComputeFreeVolumeGPU<ShapeSphinx>(m, "ComputeFreeVolumeSphinxGPU");
    export_UpdaterClustersGPU<ShapeSphinx>(m, "UpdaterClustersSphinxGPU");

#endif
#endif
    }

    } // namespace detail
    } // namespace hpmc
    } // namespace hoomd
