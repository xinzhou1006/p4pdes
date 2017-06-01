static const char help[] =
"Build and view a tiny three-triangle mesh using DMPlex.  Option prefix tny_.\n\n";

/* 
either build DMPlex via call to DMPlexCreateFromCellList() with interpolate=TRUE:
./tiny
or by directly setting cones "by hand":
./tiny -tny_by_hand

compare these views:
./tiny -dm_view
./tiny -tny_ranges
./tiny -tny_ranges -tny_use_height
./tiny -tny_cell_cones
./tiny -tny_vertex_supports
./tiny -tny_coords_view
./tiny -tny_vec_view  FIXME broken
*/

#include <petsc.h>

// Describe the mesh "triangle style" with separate numbering for cells and vertices.
static const int dim = 2,
                 ncell = 3,
                 nvert = 5,
                 cells[9] = {0, 3, 2,  // 9 = ncell * (dim+1)
                             0, 2, 1,
                             2, 3, 4};
static const double coordverts[10] = {0.0, 0.0,  // 10 = nvert * dim
                                      0.0, 1.0,
                                      0.5, 1.0,
                                      1.0, 0.0,
                                      1.0, 1.0};

// Describe same mesh, but directly as DMPlex, i.e. by giving cell and
// edge cones in DAG.  These values are generated by DMPlexCreateFromCellList()
// internally, so they are redundant if we use that create method.
static const int npoint = 15,
                 offedge = 8,
                 ccone[3][3] = {{8,9,10},
                                {10,11,12},
                                {9,13,14}},
                 econe[7][2] = {{3,6},
                                {5,6},
                                {3,5},
                                {4,5},
                                {3,4},
                                {6,7},
                                {5,7}};

extern PetscErrorCode CreateMeshByHand(DM*);
extern PetscErrorCode CreateCoordinateSectionByHand(DM*);
extern PetscErrorCode PlexViewRanges(DM, PetscBool);
extern PetscErrorCode PlexViewFans(DM, int, int, int);

int main(int argc,char **argv) {
    PetscErrorCode ierr;
    DM            dmplex;
    PetscSection  section;
    PetscBool     by_hand = PETSC_FALSE,
                  cell_cones = PETSC_FALSE,
                  coords_view = PETSC_FALSE,
                  ranges = PETSC_FALSE,
                  use_height = PETSC_FALSE,
                  vec_view = PETSC_FALSE,
                  vertex_supports = PETSC_FALSE;

    PetscInitialize(&argc,&argv,NULL,help);

    ierr = PetscOptionsBegin(PETSC_COMM_WORLD, "tny_", "options for tiny", "");CHKERRQ(ierr);
    ierr = PetscOptionsBool("-by_hand", "use by-hand construction",
                            "tiny.c", by_hand, &by_hand, NULL);CHKERRQ(ierr);
    ierr = PetscOptionsBool("-cell_cones", "print cones of each cell",
                            "tiny.c", cell_cones, &cell_cones, NULL);CHKERRQ(ierr);
    ierr = PetscOptionsBool("-coords_view", "print entries of a global vec for vertex coordinates",
                            "tiny.c", coords_view, &coords_view, NULL);CHKERRQ(ierr);
    ierr = PetscOptionsBool("-ranges", "print point index ranges for vertices,edges,cells",
                            "tiny.c", ranges, &ranges, NULL);CHKERRQ(ierr);
    ierr = PetscOptionsBool("-use_height", "use Height instead of Depth when printing points",
                            "tiny.c", use_height, &use_height, NULL);CHKERRQ(ierr);
    ierr = PetscOptionsBool("-vec_view", "print entries of a global vec for P2 elements",
                            "tiny.c", vec_view, &vec_view, NULL);CHKERRQ(ierr);
    ierr = PetscOptionsBool("-vertex_supports", "print supports of each vertex",
                            "tiny.c", vertex_supports, &vertex_supports, NULL);CHKERRQ(ierr);
    ierr = PetscOptionsEnd();

    if (by_hand) {
        ierr = CreateMeshByHand(&dmplex); CHKERRQ(ierr);
        ierr = CreateCoordinateSectionByHand(&dmplex); CHKERRQ(ierr);
    } else {
        PetscMPIInt rank;
        ierr = MPI_Comm_rank(PETSC_COMM_WORLD, &rank); CHKERRQ(ierr);
        if (rank == 0) { // create mesh on rank 0
            ierr = DMPlexCreateFromCellList(PETSC_COMM_WORLD,
                dim,ncell,nvert,dim+1,
                PETSC_TRUE,  // "interpolate" flag; TRUE means "topologically-interpolate"
                             // i.e. create edges (1D) from vertices (0D) and cells (2D)
                cells,dim,coordverts,
                &dmplex); CHKERRQ(ierr);
        } else { // empty mesh on rank > 0
            ierr = DMPlexCreateFromCellList(PETSC_COMM_WORLD,
                dim,0,0,dim+1,PETSC_TRUE,NULL,dim,NULL,&dmplex); CHKERRQ(ierr);
        }
    }
    ierr = PetscObjectSetName((PetscObject) dmplex, "tiny mesh"); CHKERRQ(ierr);

    /* distribute mesh over processes; same source as above */
    PetscPartitioner part;
    DM               distributedMesh = NULL;
    ierr = DMPlexGetPartitioner(dmplex,&part);CHKERRQ(ierr);
    ierr = PetscPartitionerSetFromOptions(part);CHKERRQ(ierr);  // allows -petscpartitioner_view
    // overlap of 0 appropriate to P2 etc. FEM:
    ierr = DMPlexDistribute(dmplex, 0, NULL, &distributedMesh);CHKERRQ(ierr);
    if (distributedMesh) {
      ierr = DMDestroy(&dmplex);CHKERRQ(ierr);
      dmplex  = distributedMesh;
    }

    ierr = DMSetFromOptions(dmplex); CHKERRQ(ierr);

    // viewing of dmplex and vertex coordinates
    ierr = DMViewFromOptions(dmplex, NULL, "-dm_view"); CHKERRQ(ierr);  // why not enabled by default?
    if (ranges) {
        ierr = PlexViewRanges(dmplex,use_height); CHKERRQ(ierr);
    }
    if (cell_cones) {
        ierr = PlexViewFans(dmplex,2,2,1); CHKERRQ(ierr);
    }
    if (vertex_supports) {
        ierr = PlexViewFans(dmplex,2,0,1); CHKERRQ(ierr);
    }
    if (coords_view) {
        Vec coords;
        ierr = DMGetCoordinates(dmplex,&coords); CHKERRQ(ierr);
        if (coords) {
            ierr = VecView(coords,PETSC_VIEWER_STDOUT_WORLD); CHKERRQ(ierr);
        } else {
            ierr = PetscPrintf(PETSC_COMM_WORLD,
                "[vertex coordinates have not been set]\n"); CHKERRQ(ierr);
        }
    }

    // create dofs like P2 elements using PetscSection
    // with 1 dof on each vertex (depth==0) and 1 dof on each edge (depth==1)
    // [DMPlexCreateSection() seems to do something like the following]
    int  j, pstart, pend, vstart, vend, estart, eend;
    ierr = DMPlexGetChart(dmplex,&pstart,&pend); CHKERRQ(ierr);
    ierr = DMPlexGetDepthStratum(dmplex, 0, &vstart, &vend); CHKERRQ(ierr);
    ierr = DMPlexGetDepthStratum(dmplex, 1, &estart, &eend); CHKERRQ(ierr);
    ierr = PetscSectionCreate(PETSC_COMM_WORLD,&section); CHKERRQ(ierr);
    ierr = PetscSectionSetChart(section, pstart, pend); CHKERRQ(ierr);
    for (j = vstart; j < vend; ++j) {
        ierr = PetscSectionSetDof(section, j, 1); CHKERRQ(ierr);
    }
    for (j = estart; j < eend; ++j) {
        ierr = PetscSectionSetDof(section, j, 1); CHKERRQ(ierr);
    }
    ierr = PetscSectionSetUp(section); CHKERRQ(ierr);
    ierr = DMSetDefaultSection(dmplex, section); CHKERRQ(ierr);

#if 0
//TO DO THE FOLLOWING, NAMELY SET VALUES ON ONE CELL, NEED TO CHECK IF WE OWN THAT CELL
    // assign values in a global Vec for the section, i.e. on P2 dofs
    // FIXME a more interesting task would be to have an f(x,y), and attach
    // coordinates to the nodes, and evaluate an integral \int_Omega f(x,y) dx dy
    Vec    v;
    double *av;
    int    m, numpts, *pts = NULL, dof, off;

    ierr = DMGetGlobalVector(dmplex, &v); CHKERRQ(ierr);
    ierr = PetscObjectSetName((PetscObject) v, "v"); CHKERRQ(ierr);
    ierr = VecSet(v,0.0); CHKERRQ(ierr);

    // FIXME Vec gets 1.0 for dofs on cell=1  <-- boring
    VecGetArray(v, &av);
    DMPlexGetTransitiveClosure(dmplex, 1, PETSC_TRUE, &numpts, &pts);
    for (j = 0; j < 2 * numpts; j += 2) {  // skip over orientations
        PetscSectionGetDof(section, pts[j], &dof);
        PetscSectionGetOffset(section, pts[j], &off);
        for (m = 0; m < dof; ++m) {
            av[off+m] = 1.0;
        }
    }
    DMPlexRestoreTransitiveClosure(dmplex, 1, PETSC_TRUE, &numpts, &pts);
    VecRestoreArray(v, &av);

    if (vec_view) {
        Vec coords;
        ierr = VecView(v,PETSC_VIEWER_STDOUT_WORLD); CHKERRQ(ierr);
        ierr = DMGetCoordinates(dmplex,&coords); CHKERRQ(ierr);
        if (coords) {
            ierr = VecView(coords,PETSC_VIEWER_STDOUT_WORLD); CHKERRQ(ierr);
        } else {
            ierr = PetscPrintf(PETSC_COMM_WORLD,
                "[vertex coordinates have not been set]\n"); CHKERRQ(ierr);
        }
    }
    ierr = DMRestoreGlobalVector(dmplex, &v); CHKERRQ(ierr);
#endif

    PetscSectionDestroy(&section); DMDestroy(&dmplex);
    return PetscFinalize();
}


/* This function is essentially equivalent to using DMPlexCreateFromCellList().
Note that rank 0 gets the actual mesh and other ranks get an empty mesh.
See the implementations of
    DMPlexBuildFromCellList_Private()
    DMPlexCreateFromCellListParallel()
    DMPlexInterpolate()
    DMPlexBuildCoordinates_Private()      */
PetscErrorCode CreateMeshByHand(DM *dmplex) {
    PetscErrorCode ierr;
    int           j;
    PetscMPIInt   rank;
    ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank); CHKERRQ(ierr);
    ierr = DMPlexCreate(PETSC_COMM_WORLD,dmplex); CHKERRQ(ierr);
    ierr = DMSetDimension(*dmplex,dim); CHKERRQ(ierr);
    if (rank == 0) {
        // set the total number of points (npoint = ncell + nvert + nedges)
        ierr = DMPlexSetChart(*dmplex, 0, npoint); CHKERRQ(ierr);
        // the points are cells, vertices, edges in that order
        // we only set cones for cells and edges
        for (j = 0; j < ncell; j++) {
            ierr = DMPlexSetConeSize(*dmplex, j, dim+1); CHKERRQ(ierr);
        }
        for (j = offedge; j < npoint; j++) {
            ierr = DMPlexSetConeSize(*dmplex, j, dim); CHKERRQ(ierr);
        }
        ierr = DMSetUp(*dmplex);
        for (j = 0; j < ncell; j++) {
            ierr = DMPlexSetCone(*dmplex, j, ccone[j]); CHKERRQ(ierr);
        }
        for (j = offedge; j < npoint; j++) {
            ierr = DMPlexSetCone(*dmplex, j, econe[j-offedge]); CHKERRQ(ierr);
        }
    } else {
        ierr = DMPlexSetChart(*dmplex, 0, 0); CHKERRQ(ierr);
    }
    // with cones we have only upward directions and no labels for the strata
    // (note: both Symmetrize & Stratify are required, and they must be in this order
    ierr = DMPlexSymmetrize(*dmplex); CHKERRQ(ierr);
    ierr = DMPlexStratify(*dmplex); CHKERRQ(ierr);
    return 0;
}

// FIXME won't work if rank>0 ??
PetscErrorCode CreateCoordinateSectionByHand(DM *dmplex) {
    PetscErrorCode ierr;
    PetscSection  coordSection;
    DM            cdm;
    Vec           coordinates;
    double        *acoord;
    int           j, d;
    // you have to setup the PetscSection returned by DMGetCoordinateSection() first,
    // or else the Vec returned by DMCreateLocalVector() has zero size
    // (and thus seg faults)
    ierr = DMGetCoordinateSection(*dmplex, &coordSection); CHKERRQ(ierr);
    ierr = PetscSectionSetNumFields(coordSection, 1); CHKERRQ(ierr);
    ierr = PetscSectionSetFieldComponents(coordSection, 0, dim); CHKERRQ(ierr);
    ierr = PetscSectionSetChart(coordSection, ncell, ncell+nvert); CHKERRQ(ierr); // FIXME use zero sizes if rank>0 ?
    for (j = ncell; j < ncell+nvert; j++) {
        ierr = PetscSectionSetDof(coordSection, j, dim); CHKERRQ(ierr);
        ierr = PetscSectionSetFieldDof(coordSection, j, 0, dim); CHKERRQ(ierr);
    }
    ierr = PetscSectionSetUp(coordSection); CHKERRQ(ierr);
    // now we can actually set up the coordinate Vec
    ierr = DMGetCoordinateDM(*dmplex, &cdm); CHKERRQ(ierr);
    ierr = DMCreateLocalVector(cdm, &coordinates); CHKERRQ(ierr);
    ierr = VecSetBlockSize(coordinates,dim); CHKERRQ(ierr);
    ierr = PetscObjectSetName((PetscObject) coordinates, "coordinates"); CHKERRQ(ierr);
    ierr = VecGetArray(coordinates, &acoord); CHKERRQ(ierr);
    for (j = 0; j < nvert; j++) {
        for (d = 0; d < dim; ++d) {
            acoord[j*dim+d] = coordverts[j*dim+d];
        }
    }
    ierr = VecRestoreArray(coordinates, &acoord); CHKERRQ(ierr);
    // finally we tell the DM that it has coordinates
    ierr = DMSetCoordinatesLocal(*dmplex, coordinates); CHKERRQ(ierr);
    VecDestroy(&coordinates);
    return 0;
}

static const char* stratanames[4][10] =
                       {{"vertex","",    "",    ""},       // dim=0 names
                        {"vertex","cell","",    ""},       // dim=1 names
                        {"vertex","edge","cell",""},       // dim=2 names
                        {"vertex","edge","face","cell"}};  // dim=3 names

PetscErrorCode PlexViewRanges(DM plex, PetscBool use_height) {
    PetscErrorCode ierr;
    int         dim, m, start, end;
    MPI_Comm    comm;
    PetscMPIInt rank,size;
    ierr = PetscObjectGetComm((PetscObject)plex,&comm); CHKERRQ(ierr);
    ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
    ierr = MPI_Comm_rank(comm,&rank); CHKERRQ(ierr);
    ierr = DMGetDimension(plex,&dim); CHKERRQ(ierr);
    if (size > 1) {
        ierr = PetscSynchronizedPrintf(comm,"[rank %d] ",rank); CHKERRQ(ierr);
    }
    ierr = DMPlexGetChart(plex,&start,&end); CHKERRQ(ierr);
    ierr = PetscSynchronizedPrintf(comm,
        "chart for %d-dimensional DMPlex has points %d,...,%d\n",
        dim,start,end-1); CHKERRQ(ierr);
    for (m = 0; m < dim + 1; m++) {
        if (use_height) {
            ierr = DMPlexGetHeightStratum(plex,m,&start,&end); CHKERRQ(ierr);
            ierr = PetscSynchronizedPrintf(comm,
                "    height %d of size %d: %d,...,%d (%s)\n",
                m,end-start,start,end-1,dim < 4 ? stratanames[dim][2-m] : ""); CHKERRQ(ierr);
        } else {
            ierr = DMPlexGetDepthStratum(plex,m,&start,&end); CHKERRQ(ierr);
            ierr = PetscSynchronizedPrintf(comm,
                "    depth=dim %d of size %d: %d,...,%d (%s)\n",
                m,end-start,start,end-1,dim < 4 ? stratanames[dim][m] : ""); CHKERRQ(ierr);
        }
    }
    ierr = PetscSynchronizedFlush(comm,PETSC_STDOUT); CHKERRQ(ierr);
    return 0;
}

/* viewing cell cones in 2D:
     PlexViewFans(dmplex,2,2,1)
viewing vertex supports:
     PlexViewFans(dmplex,2,0,1)   */
PetscErrorCode PlexViewFans(DM plex, int dim, int basestrata, int targetstrata) {
    PetscErrorCode ierr;
    const int   *targets;
    int         j, m, start, end, cssize;
    MPI_Comm    comm;
    PetscMPIInt rank,size;
    ierr = PetscObjectGetComm((PetscObject)plex,&comm); CHKERRQ(ierr);
    ierr = MPI_Comm_size(comm,&size);CHKERRQ(ierr);
    ierr = MPI_Comm_rank(comm,&rank); CHKERRQ(ierr);
    if (size > 1) {
        ierr = PetscSynchronizedPrintf(comm,"[rank %d] ",rank); CHKERRQ(ierr);
    }
    ierr = PetscSynchronizedPrintf(comm,
        "%s (= %s indices) of each %s:\n",
        (basestrata > targetstrata) ? "cones" : "supports",
        stratanames[dim][targetstrata],stratanames[dim][basestrata]); CHKERRQ(ierr);
    ierr = DMPlexGetDepthStratum(plex, basestrata, &start, &end); CHKERRQ(ierr);
    for (m = start; m < end; m++) {
        if (basestrata > targetstrata) {
            ierr = DMPlexGetConeSize(plex,m,&cssize); CHKERRQ(ierr);
            ierr = DMPlexGetCone(plex,m,&targets); CHKERRQ(ierr);
        } else {
            ierr = DMPlexGetSupportSize(plex,m,&cssize); CHKERRQ(ierr);
            ierr = DMPlexGetSupport(plex,m,&targets); CHKERRQ(ierr);
        }
        ierr = PetscSynchronizedPrintf(comm,
            "    %s %d: ",stratanames[dim][basestrata],m); CHKERRQ(ierr);
        for (j = 0; j < cssize-1; j++) {
            ierr = PetscSynchronizedPrintf(comm,
                "%d,",targets[j]); CHKERRQ(ierr);
        }
        ierr = PetscSynchronizedPrintf(comm,
            "%d\n",targets[cssize-1]); CHKERRQ(ierr);
    }
    ierr = PetscSynchronizedFlush(comm,PETSC_STDOUT); CHKERRQ(ierr);
    return 0;
}
