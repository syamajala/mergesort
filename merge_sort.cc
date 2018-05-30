#include <cstdlib>
#include <ctime>
#include "legion.h"

using namespace Legion;

enum TaskID {
  TOP_LEVEL_TASK_ID,
};

enum FieldIDs {
  FID_X
};

void top_level_task(const Task *task,
                    const std::vector<PhysicalRegion> &regions,
                    Context ctx,
                    Runtime *runtime)
{
  // Allocate regions and launch merge sort task

  int num_elemnts = 1024;

  const InputArgs &command_args = Runtime::get_input_args();
  for (int i = 1; i < command_args.argc; i++)
  {
    if(!strcmp(command_args.argv[i], "-n"))
    {
      num_elemnts = atoi(command_args.argv[i]);
      assert(num_elemnts >= 0);
    }
  }

  Rect<1> elem_rect(0, num_elemnts-1);
  IndexSpace is = runtime->create_index_space(ctx, elem_rect);

  FieldSpace input_fs = runtime->create_field_space(ctx);

  {
    FieldAllocator allocator = runtime->create_field_allocator(ctx, input_fs);
    allocator.allocate_field(sizeof(int), FID_X);
  }

  LogicalRegion input_lr = runtime->create_logical_region(ctx, is, input_fs);

  RegionRequirement req(input_lr, READ_WRITE, EXCLUSIVE, input_lr);
  req.add_field(FID_X);

  InlineLauncher input_launcher(req);
  PhysicalRegion input_region = runtime->map_region(ctx, input_launcher);

  const FieldAccessor<READ_WRITE, int, 1> acc_x(input_region, FID_X);

  std::srand(std::time(nullptr));

  for (PointInRectIterator<1> pir(elem_rect); pir(); pir++)
  {
    acc_x[*pir] = std::rand();
  }

  // launch top_down_merge_sort task
}


/*
Top-down implementation
Example C-like code using indices for top down merge sort algorithm that recursively splits the list
(called runs in this example) into sublists until sublist size is 1, then merges those sublists to produce a
sorted list. The copy back step is avoided with alternating the direction of the merge with each level of
recursion.

// Array A[] has the items to sort; array B[] is a work array.
TopDownMergeSort(A[], B[], n)
{
    CopyArray(A, 0, n, B);           // duplicate array A[] into B[]
    TopDownSplitMerge(B, 0, n, A);   // sort data from B[] into A[]
}
*/
void top_down_merge_sort(const Task *task,
                         const std::vector<PhysicalRegion> &regions,
                         Context ctx,
                         Runtime *runtime)
{

}

/*
Sort the given run of array A[] using array B[] as a source.
iBegin is inclusive; iEnd is exclusive (A[iEnd] is not in the set).
TopDownSplitMerge(B[], iBegin, iEnd, A[])
{
    if(iEnd - iBegin < 2)                       // if run size == 1
        return;                                 //   consider it sorted
    // split the run longer than 1 item into halves
    iMiddle = (iEnd + iBegin) / 2;              // iMiddle = mid point
    // recursively sort both runs from array A[] into B[]
    TopDownSplitMerge(A, iBegin,  iMiddle, B);  // sort the left  run
    TopDownSplitMerge(A, iMiddle,    iEnd, B);  // sort the right run
    // merge the resulting runs from array B[] into A[]
    TopDownMerge(B, iBegin, iMiddle, iEnd, A);
}
*/
void top_down_split_merge(const Task *task,
                          const std::vector<PhysicalRegion> &regions,
                          Context ctx,
                          Runtime *runtime)
{

}

/*
//  Left source half is A[ iBegin:iMiddle-1].
// Right source half is A[iMiddle:iEnd-1   ].
// Result is            B[ iBegin:iEnd-1   ].
TopDownMerge(A[], iBegin, iMiddle, iEnd, B[])
{
    i = iBegin, j = iMiddle;
    // While there are elements in the left or right runs...
    for (k = iBegin; k < iEnd; k++) {
        // If left run head exists and is <= existing right run head.
        if (i < iMiddle && (j >= iEnd || A[i] <= A[j])) {
            B[k] = A[i];
            i = i + 1;
        } else {
            B[k] = A[j];
            j = j + 1;
        }
    }
}

*/
void top_down_merge(const Task *task,
                    const std::vector<PhysicalRegion> &regions,
                    Context ctx,
                    Runtime *runtime)
{

}



int main(int argc, char **argv)
{
  Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);

  {
    TaskVariantRegistrar registrar(TOP_LEVEL_TASK_ID, "top_level");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<top_level_task>(registrar, "top_level");
  }

  return Runtime::start(argc, argv);
}
