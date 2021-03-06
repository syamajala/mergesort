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
  int iMiddle;
  int iEnd;

  Args(int iBegin, int iMiddle, int iEnd) :
    iBegin  ( iBegin ),
    iMiddle ( iMiddle ),
    iEnd    ( iEnd )
  {};
};

void top_level_task(const Task *task,
                    const std::vector<PhysicalRegion> &regions,
                    Context ctx,
                    Runtime *runtime)
{
  // Allocate regions and launch merge sort task
  // int num_elements = 1024;
  int num_elements = 4;
  int elements[4] = {8, 4, 3, 1};

  const InputArgs &command_args = Runtime::get_input_args();
  for (int i = 1; i < command_args.argc; i++)
  {
    if(!strcmp(command_args.argv[i], "-n"))
    {
      num_elements = atoi(command_args.argv[++i]);
      assert(num_elements >= 0);
    }
  }
  std::cout << "Num Elements: " << num_elements << std::endl;

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

  LogicalRegion output_lr = runtime->create_logical_region(ctx, is, output_fs);

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
  std::cout << "Populating: [";
  int i = 0;
  for (PointInRectIterator<1> pir(elem_rect); pir(); pir++)
  {
    // int e = std::rand();
    int e = elements[i];
    acc_x[*pir] = e;
    acc_y[*pir] = e;
    std::cout << acc_x[*pir] << ", ";
    i++;
  }
  std::cout << "]"<< std::endl;

  // launch top_down_merge_sort task

  Args args(0, 0, num_elements);
  TaskLauncher merge_sort(TOP_DOWN_SPLIT_MERGE_TASK_ID, TaskArgument(&args, sizeof(Args)));

  merge_sort.add_region_requirement(RegionRequirement(input_lr, READ_ONLY, EXCLUSIVE, input_lr));
  merge_sort.region_requirements[0].add_field(FID_X);

  merge_sort.add_region_requirement(RegionRequirement(output_lr, READ_WRITE, EXCLUSIVE, output_lr));
  merge_sort.region_requirements[1].add_field(FID_Y);

  runtime->execute_task(ctx, merge_sort);

  std::cout << "Output: ";
  for (PointInRectIterator<1> pir(elem_rect); pir(); pir++)
  {
    std::cout << acc_y[*pir] << " ";
  }
  std::cout << std::endl;
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
  assert(task->regions.size() == 2);

  int iBegin = 0;
  int iEnd   = 0;

  if (task->args != NULL)
  {
    assert(task->arglen == sizeof(Args));
    iBegin = ((const Args*)task->args)->iBegin;
    iEnd   = ((const Args*)task->args)->iEnd;
  }
  else
  {
    assert(task->local_arglen == sizeof(Args));
    iBegin = ((const Args*)task->local_args)->iBegin;
    iEnd   = ((const Args*)task->local_args)->iEnd;
  }

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

  Args args_left(iBegin, 0, iMiddle);
  arg_map.set_point(0, TaskArgument(&args_left, sizeof(Args)));

  Args args_right(iMiddle, 0, iEnd);
  arg_map.set_point(1, TaskArgument(&args_right, sizeof(Args)));

  IndexLauncher index_launcher(TOP_DOWN_SPLIT_MERGE_TASK_ID, color_is, TaskArgument(NULL, 0), arg_map);

  index_launcher.add_region_requirement(RegionRequirement(input_lp, 0, READ_ONLY, EXCLUSIVE, input_lr));
  index_launcher.region_requirements[0].add_field(FID_X);

  index_launcher.add_region_requirement(RegionRequirement(output_lp, 0, READ_WRITE, EXCLUSIVE, output_lr));
  index_launcher.region_requirements[1].add_field(FID_Y);

  runtime->execute_index_space(ctx, index_launcher);

  // launch top down merge
  Args merge_args(iBegin, iMiddle, iEnd);

  TaskLauncher top_down_merge_task(TOP_DOWN_MERGE_TASK_ID, TaskArgument(&merge_args, sizeof(Args)));

  top_down_merge_task.add_region_requirement(RegionRequirement(input_lr, READ_ONLY, EXCLUSIVE, input_lr));
  top_down_merge_task.region_requirements[0].add_field(FID_X);

  top_down_merge_task.add_region_requirement(RegionRequirement(output_lr, READ_WRITE, EXCLUSIVE, output_lr));
  top_down_merge_task.region_requirements[1].add_field(FID_Y);

  runtime->execute_task(ctx, top_down_merge_task);
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
  assert(task->arglen == sizeof(Args));

  int iBegin  = ((const Args*)task->args)->iBegin;
  int iMiddle = ((const Args*)task->args)->iMiddle;
  int iEnd    = ((const Args*)task->args)->iEnd;
  int i       = iBegin;
  int j       = iMiddle;

  std::cout << "iBegin: " << iBegin << " iMiddle: " << iMiddle << " iEnd: " << iEnd << std::endl;

  FieldID input_fid = *(task->regions[0].privilege_fields.begin());
  const FieldAccessor<READ_ONLY,int,1> acc_x(regions[0], input_fid);

  FieldID output_fid = *(task->regions[1].privilege_fields.begin());
  const FieldAccessor<READ_WRITE,int,1> acc_y(regions[1], output_fid);

  Rect<1> rect = runtime->get_index_space_domain(ctx, task->regions[0].region.get_index_space());
  for (PointInRectIterator<1> pir(rect); pir(); pir++)
  {
    if (i < iMiddle && (j >= iEnd || acc_x[Point<1>(i)] <= acc_x[Point<1>(j)]))
    {
      int x = acc_x[Point<1>(i)];
      acc_y[*pir] = x;
      i++;
    }
    else
    {
      int x = acc_x[Point<1>(j)];
      acc_y[*pir] = x;
      j++;
    }
    std::cout << acc_y[*pir] << " ";
  }
  std::cout << std::endl;
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
