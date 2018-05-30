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
}

void top_down_merge_sort(const Task *task,
                         const std::vector<PhysicalRegion> &regions,
                         Context ctx,
                         Runtime *runtime)
{

}

void top_down_split_merge(const Task *task,
                          const std::vector<PhysicalRegion> &regions,
                          Context ctx,
                          Runtime *runtime)
{

}

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
