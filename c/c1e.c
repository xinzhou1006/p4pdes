static char help[] = "Compute e in parallel with PETSc.\n\n";

#include <petscsys.h>

int main(int argc,char **args) {
  PetscErrorCode  ierr;
  PetscMPIInt     rank;
  PetscScalar     localval, globalsum;
  int             i;

  PetscInitialize(&argc,&args,(char*)0,help);  // <-- always call

  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank); CHKERRQ(ierr);

  // compute  1 / n!  where n = (one more than rank of process)
  localval = 1.0;
  for (i = 1; i < rank; i++)
    localval *= i+1;
  localval = 1.0 / localval;

  // sum the contributions over all processes
  ierr = MPI_Allreduce(&localval, &globalsum, 1, MPI_DOUBLE, MPI_SUM,
                       PETSC_COMM_WORLD); CHKERRQ(ierr);

  // output one estimate of e
  ierr = PetscPrintf(PETSC_COMM_WORLD,
                     "e is about %17.15f\n",globalsum); CHKERRQ(ierr);

  // from each process, output report on work done
  ierr = PetscPrintf(PETSC_COMM_SELF,
                     "rank %d did %d flops\n",rank,2 * rank + 1); CHKERRQ(ierr);

  PetscFinalize();  // <-- always call
  return 0;
}