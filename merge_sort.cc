#include <iostream>
#include <cstdlib>
#include <ctime>
#include "legion.h"

using namespace Legion;

enum TaskID {
  TOP_LEVEL_TASK_ID,
  TOP_DOWN_SPLIT_MERGE_TASK_ID,
  TOP_DOWN_MERGE_TASK_ID

};

enum FieldIDs {
  FID_X,
  FID_Y
};

struct Args {
  int iBegin;
  int iEnd;

  Args(int iBegin, int iEnd) :
    iBegin ( iBegin ),
    iEnd   ( iEnd )
  {};
};

void top_level_task(const Task *task,
                    const std::vector<PhysicalRegion> &regions,
                    Context ctx,
                    Runtime *runtime)
{
  // Allocate regions and launch merge sort task

  int num_elements = 1024;

  const InputArgs &command_args = Runtime::get_input_args();
  for (int i = 1; i < command_args.argc; i++)
  {
    if(!strcmp(command_args.argv[i], "-n"))
    {
      num_elements = atoi(command_args.argv[i]);
      assert(num_elements >= 0);
    }
  }

  Rect<1> elem_rect(0, num_elements-1);
  IndexSpace is = runtime->create_index_space(ctx, elem_rect);

  FieldSpace input_fs = runtime->create_field_space(ctx);

  {
    FieldAllocator allocator = runtime->create_field_allocator(ctx, input_fs);
    allocator.allocate_field(sizeof(int), FID_X);
  }

  FieldSpace output_fs = runtime->create_field_space(ctx);

  {
    FieldAllocator allocator = runtime->create_field_allocator(ctx, output_fs);
    allocator.allocate_field(sizeof(int), FID_Y);
  }

  LogicalRegion input_lr = runtime->create_logical_region(ctx, is, input_fs);
  runtime->attach_name(input_lr, "input_lr");

  LogicalRegion output_lr = runtime->create_logical_region(ctx, is, output_fs);
  runtime->attach_name(output_lr, "output_lr");

  RegionRequirement input_req(input_lr, READ_WRITE, EXCLUSIVE, input_lr);
  input_req.add_field(FID_X);

  InlineLauncher input_launcher(input_req);
  PhysicalRegion input_region = runtime->map_region(ctx, input_launcher);

  RegionRequirement output_req(output_lr, READ_WRITE, EXCLUSIVE, output_lr);
  output_req.add_field(FID_Y);

  InlineLauncher output_launcher(output_req);
  PhysicalRegion output_region = runtime->map_region(ctx, output_launcher);

  const FieldAccessor<READ_WRITE, int, 1> acc_x(input_region, FID_X);
  const FieldAccessor<READ_WRITE, int, 1> acc_y(output_region, FID_Y);

  std::srand(std::time(nullptr));

  for (PointInRectIterator<1> pir(elem_rect); pir(); pir++)
  {
    acc_x[*pir] = std::rand();
    acc_y[*pir] = acc_x[*pir];
  }

  // launch top_down_merge_sort task
  Args args(0, num_elements);
  TaskLauncher merge_sort(TOP_DOWN_SPLIT_MERGE_TASK_ID, TaskArgument(&args, sizeof(Args)));

  merge_sort.add_region_requirement(RegionRequirement(input_lr, READ_ONLY, EXCLUSIVE, input_lr));
  merge_sort.region_requirements[0].add_field(FID_X);

  merge_sort.add_region_requirement(RegionRequirement(output_lr, READ_WRITE, EXCLUSIVE, output_lr));
  merge_sort.region_requirements[1].add_field(FID_Y);

  runtime->execute_task(ctx, merge_sort);
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
  // assert(task->arglen == sizeof(Args));
  assert(task->regions.size() == 2);

  int iBegin = ((const Args*)task->args)->iBegin;
  int iEnd = ((const Args*)task->args)->iEnd;

  if (iEnd - iBegin < 2)
  {
    return;
  }

  int iMiddle = (iEnd + iBegin) / 2;

  Rect<1> color_bounds(0, 1);
  IndexSpace color_is = runtime->create_index_space(ctx, color_bounds);

  IndexSpace is = task->regions[0].region.get_index_space();

  IndexPartition ip = runtime->create_equal_partition(ctx, is, color_is);

  LogicalRegion input_lr = task->regions[0].region;
  LogicalPartition input_lp = runtime->get_logical_partition(ctx, input_lr, ip);

  LogicalRegion output_lr = task->regions[1].region;
  LogicalPartition output_lp = runtime->get_logical_partition(ctx, output_lr, ip);

  ArgumentMap arg_map;

  Args args_left(iBegin, iMiddle);
  arg_map.set_point(0, TaskArgument(&args_left, sizeof(Args)));

  Args args_right(iMiddle, iEnd);
  arg_map.set_point(1, TaskArgument(&args_right, sizeof(Args)));

  IndexLauncher index_launcher(TOP_DOWN_SPLIT_MERGE_TASK_ID, color_is, TaskArgument(NULL, 0), arg_map);
  index_launcher.add_region_requirement(RegionRequirement(input_lp, 0, READ_ONLY, EXCLUSIVE, input_lr));
  index_launcher.region_requirements[0].add_field(FID_X);
  index_launcher.add_region_requirement(RegionRequirement(output_lp, 0, READ_WRITE, EXCLUSIVE, output_lr));
  index_launcher.region_requirements[1].add_field(FID_Y);

  runtime->execute_index_space(ctx, index_launcher);

  // launch top down merge
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

  {
    TaskVariantRegistrar registrar(TOP_DOWN_SPLIT_MERGE_TASK_ID, "top_down_split_merge");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<top_down_split_merge>(registrar, "top_down_split_merge");
  }

  {
    TaskVariantRegistrar registrar(TOP_DOWN_MERGE_TASK_ID, "top_down_merge");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<top_down_merge>(registrar, "top_down_merge");
  }

  return Runtime::start(argc, argv);
}
