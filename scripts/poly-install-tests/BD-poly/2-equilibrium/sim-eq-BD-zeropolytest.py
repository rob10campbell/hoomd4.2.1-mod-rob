## mpi simulation to resolve overlaps from init-BD.gsd
## Brings a Brownian Dynamics simulation of attractive colloids
## to thermal equilibrium and creates Equilibrium-BD.gsd
## NOTE: requires matching init-BD.gsd file
##       for a variable number of colloids and FIXED BOX SIZE 
## (Rob Campbell)


#########  MODULE LIBRARY
# Use HOOMD-blue
import hoomd
import hoomd.md # molecular dynamics
# Use GSD files
import gsd # provides Python API; MUST import sub packages explicitly (as below)
import gsd.hoomd # read and write HOOMD schema GSD files
# Maths
import numpy
import math
import random # psuedo-random number generator
# Other
import os # miscellaneous operating system interfaces


######### CUSTOM CLASSES
#N/A


######### SIMULATION INPUTS
# General parameters
phi =  0.2 # volume fraction
percent_C1 = 1.00
percent_C2 = 0.00
poly_C1 = 0.05
poly_C2 = 0.00
rho = 3 # number density (per unit volume)
KT = 0.1 # system temperature
D0 = 0.0 * KT # attraction strength (gels at >=4kT)
kappa = 30.0 # range of attraction (4 (long range)- 30 (short range)), distance in BD units is approx 3/kappa 

N_time_steps = 10000 #100000 # number  of  time steps
dt_Integration = 0.001 # dt! (BD timestep, may need to be smaller than DPD)
period = 100 #10000 # recording interval

# Simulation box size (fixed by L_X)
L_X = 30
L_Y = L_X
L_Z = L_X
V_total = L_X * L_Y * L_Z # total volume of simulation box (cube)

# Colloid particle details
R_C1 = 1 # 1st type colloid particle radius
V_C1 = (4./3.) * math.pi * R_C1 ** 3 # 1st type colloid particle volume (1 particle)
m_C1 = V_C1 * rho # 1st type colloid particle mass
V_Colloids_type1 = percent_C1 * phi * V_total # total volume of type 1 colloids
N_C1 = round(V_Colloids_type1 / V_C1) # number of 1st type of colloid particles (INT)

R_C2 = 2 # 2nd type colloid particle radius
V_C2 = (4./3.) * math.pi * R_C2 ** 3 # 2nd type colloid particle volume (1 particle)
m_C2 = V_C2 * rho # 2st type colloid particle mass
V_Colloids_type2 = percent_C2 * phi * V_total # total volume of type 1 colloids
N_C2 = round(V_Colloids_type2 / V_C2) # number of 1st type of colloid particles (INT)

# colloids totals NOTE: ASSUMES 2 colloid types
N_C = N_C1 + N_C2 # total number of colloids
V_Colloids = V_Colloids_type1 + V_Colloids_type2 # total volume of all colloids

# Brownian parameters
eta0 = 1.0 # viscosity of the fluid (tunable parameter, not direct viscosity)
gamma = 6.0*numpy.pi*eta0*R_C1 # BD stock friction coefficient

# Particle interaction parameters
r_c = 1.0 # cut-off radius parameter, r_c>=3/kappa (r_cut = # * r_c) 
if r_c < (3/kappa):
  print('WARNING: r_c is less than range of attraction. Increase r_c')
r0 = 0.0 # minimum inter-particle distance
f_contact = 10000.0 * KT / r_c # set colloid-colloid hard-sphere interactions 

# Total number of particles in the simulation
N_total = int(N_C)


######### SIMULATION
## Checks for existing equilibrium files. If none are found, brings the 
## initial random distribution of particles to thermal equilibrium. 

if os.path.exists('Equilibrium-poly-BD.gsd'):
  print("Equilibrium file already exists. No new files created.")
else:
  print("Brownian Dynamics initialization state is being brought to equilibrium")
  ## Create a CPU simulation
  device = hoomd.device.CPU()
  sim = hoomd.Simulation(device=device, seed=50) # set seed to a fixed value for reproducible simulations

  # start the simulation from the initialized system
  sim.timestep = 0 # set initial timestep to 0
  sim.create_state_from_gsd(filename='../1-initialize/init-poly-BD.gsd')

  # assign particle types to groups 
  # (in case we want to integrate over subpopulations only, 
  # but would require other mods to source code)
  groupA = hoomd.filter.Type(['A'])
  groupB = hoomd.filter.Type(['B'])
  all_ = hoomd.filter.Type(['A','B'])

  # thermalize (aka bring to thermal equilibrium) the system
  sim.state.thermalize_particle_momenta(filter=all_, kT=KT)

  # create neighboring list
  nl = hoomd.md.nlist.Tree(buffer=0.05);

  # define Morse force (attraction) interactions
  morse = hoomd.md.pair.Morse(nlist=nl, default_r_cut=1.0 * r_c)

  # colloid-colloid: hard particles (no deformation/overlap)
  #morse.params[('A','A')] = dict(D0=D0, alpha=kappa, r0=(R_C1+R_C1))
  morse.params[('A','A')] = dict(D0=D0, alpha=kappa, r0=(R_C1+R_C1), poly=poly_C1)	
  morse.r_cut[('A','A')] = r_c+(R_C1+R_C1) # used to assemble nl

  #morse.params[('A','B')] = dict(D0=D0, alpha=kappa, r0=(R_C1+R_C2))
  morse.params[('A','B')] = dict(D0=D0, alpha=kappa, r0=(R_C1+R_C2), poly=poly_C1+poly_C2)	
  morse.r_cut[('A','B')] = r_c+(R_C1+R_C2) # used to assemble nl

  #morse.params[('B','B')] = dict(D0=D0, alpha=kappa, r0=(R_C2+R_C1))
  morse.params[('B','B')] = dict(D0=D0, alpha=kappa, r0=(R_C2+R_C1), poly=poly_C2)	
  morse.r_cut[('B','B')] = r_c+(R_C2+R_C2) # used to assemble nl


  # choose integration method for the end of each timestep
  # BROWNIAN (overdamped) or LANGEVIN (underdamped)
  brownian = hoomd.md.methods.Brownian(filter=all_, kT=KT, default_gamma=gamma)
  integrator=hoomd.md.Integrator(dt=dt_Integration, forces=[morse], methods=[brownian])
  #langevin = hoomd.md.methods.Langevin(filter=all_, kT=KT, default_gamma=gamma)
  #integrator = hoomd.md.Integrator(dt=dt_Integration, forces=[morse], methods=[langevin])
  sim.operations.integrator = integrator

  # set the simulation to log certain values
  logger = hoomd.logging.Logger()
  thermodynamic_properties = hoomd.md.compute.ThermodynamicQuantities(filter=all_)
  sim.operations.computes.append(thermodynamic_properties)
  logger.add(thermodynamic_properties, quantities=['kinetic_temperature', 
    'pressure_tensor', 'virial_ind_tensor', 'potential_energy'])
  logger.add(sim, quantities=['tps'])

  # set output file
  gsd_writer = hoomd.write.GSD(trigger=period, filename="Equilibrium-poly-BD.gsd", 
    filter=all_, mode='wb', dynamic=['property','momentum','attribute'])

  # save diameters
  gsd_writer.write_diameter = True

  # [optional] set buffer size (how often data is saved to the file, large buffers can increase performace, but can lead to lost data if sim is cancelled or times-out)
  #gsd_writer.maximum_write_buffer_size = 1e8 # max 100 million bytes

  # save outputs
  sim.operations.writers.append(gsd_writer)
  gsd_writer.logger = logger

  # run simulation!
  # (and write the initial state (e.g. the last frame of Equilibrium) in this file!)
  sim.run(N_time_steps, write_at_start=True)

  print("New Brownian Dynamics equilibrium state (Equilibrium-poly-BD.gsd) created.")
