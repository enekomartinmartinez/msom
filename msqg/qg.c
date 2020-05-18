/**
# Multiple scale quasi geostrophic model

This is the driver file for qg.h.  We define the grid, the topography,
the initial conditions and the output routines.

compile with (openmp)
qcc -lm qg.c -O3 -o qg.e -fopenmp (-llapacke)
export OMP_NUM_THREADS=20 (?)
./qg.e

MPI:
CC99='mpicc -std=c99' qcc -D_MPI=1 -lm -O3 qg.c -o qg.e -grid=multigrid (-llapacke)
mpirun -np 16 ./qg.e

HPC:
qcc -D_MPI=1 -grid=multigrid -source qg.c     (-D MKL)
rsync _qg.c
mpicc -Wall -std=c99 -O2 _qg.c -lm -o qg.e    (-mkl)
*/

#include "grid/multigrid.h"
#include "auxiliar_input.h"
#include "qg.h"
#include "qg_energy.h"
#include "qg_bfn.h"

int main(int argc,char* argv[]) {
  //double time_out;
  sscanf(argv[2],"%lf",&tend);
  sscanf(argv[1],"%s",&dpath);
  // Seardpath = ch for the configuration file with a given path or read params.in 
  read_params(strcat(argv[1], "params.in"));
  //set_path_time(argv[1], time_out)

  init_grid (N);
  size(L0);
  run();
}

/**
   Initial conditions
*/
event init (i = 0) {

  FILE * fp;
  char name[80];
  sprintf (name,"%sp_in.bas", dpath);
  if ((fp = fopen(name, "r"))) {
    input_matrixl (pol, fp);
    fclose(fp);
  } else {
  foreach() 
    for (scalar po in pol)
      po[] = 1e-3*noise();
  }
  boundary(pol);
  // invert PV at the end of other init event
}

/**
   Write parameters
 */
event write_const (t = 0) {
  backup_config();
}

event writestdout (i++) {
/* event writestdout (i=1) { */
  scalar po = pol[0];
  double ke = 0;
  foreach(reduction(+:ke))
    ke -= 0.5*po[]*laplacian(po)*sq(Delta);

  fprintf (stdout,"i = %i, dt = %g, t = %g, ke_1 = %g\n", i, dt, t, ke);
}


event output (t = tend) {
  fprintf(stdout,"write file\n");

  invertq(pol,qol);

  char name[80];
  sprintf (name,"%sp_out.bas", dpath);
  write_field(pol, name, 0.);
}

/* event adapt (t+=10) { */
/*  astats s = adapt_wavelet (pol, (double []){1e0, 1e0}, maxlevel = 9); */
/*  fprintf (ferr, "# refined %d cells, coarsened %d cells\n", s.nf, s.nc); */
/* } */
