/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "async/async.hpp"
#include "async/work_queue.hpp"
#include "compute_test_long.hpp"
#include "computer.hpp"
#include "exception.hpp"

struct testComputeParam
{
    AFK_Computer *computer;
    cl_kernel kernel;
};

void testComputeInternal(AFK_Computer *computer, cl_kernel kernel, unsigned int id)
{
    cl_context ctxt;
    cl_command_queue q;
    cl_int error;

    const int size = 10000;
    float* src_a_h = new float[size];
    float* src_b_h = new float[size];
    float* res_h = new float[size];
    // Initialize both vectors
    for (int i = 0; i < size; i++) {
        src_a_h[i] = src_b_h[i] = (float) i * (float)id;
    }

    computer->lock(ctxt, q);

    cl_mem src_a_b = clCreateBuffer(ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size * sizeof(float), src_a_h, &error);
    AFK_HANDLE_CL_ERROR(error);

    cl_mem src_b_b = clCreateBuffer(ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size * sizeof(float), src_b_h, &error);
    AFK_HANDLE_CL_ERROR(error);
    
    cl_mem res_b = clCreateBuffer(ctxt, CL_MEM_WRITE_ONLY, size * sizeof(float), NULL, &error);
    AFK_HANDLE_CL_ERROR(error);

    AFK_CLCHK(clSetKernelArg(kernel, 0, sizeof(cl_mem), &src_a_b))
    AFK_CLCHK(clSetKernelArg(kernel, 1, sizeof(cl_mem), &src_b_b))
    AFK_CLCHK(clSetKernelArg(kernel, 2, sizeof(cl_mem), &res_b))
    AFK_CLCHK(clSetKernelArg(kernel, 3, sizeof(int), &size))

    /* TODO Tweak the doodahs ? 
     * - For real compute kernels, I need to sort out a way of
     * picking correct local and global work sizes, and splitting
     * work into multiple kernels if necessary.  Different devices
     * have different characteristics: for example I've noticed
     * the GTX 570 is okay with a local_ws of 512, but the
     * HD 5850 is not; a local_ws of 64 is okay on the HD 5850
     * too.
     * Look at the `local_work_size' argument documentation for
     * clEnqueueNDRangeKernel().
     */
    size_t local_ws = 64;
    size_t global_ws = size + (64 - (size % 64));
    AFK_CLCHK(clEnqueueNDRangeKernel(q, kernel, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL))

    computer->unlock();

    float *readBack = new float[size];
    AFK_CLCHK(clEnqueueReadBuffer(q, res_b, CL_TRUE, 0, size * sizeof(float), readBack, 0, NULL, NULL))
    std::cout << "AFK_Computer: Completed execution of test program" << std::endl;

    std::cout << "AFK_Computer: Start of readback: ";
    for (int i = 0; i < 10; ++i)
        std::cout << readBack[i] << " ";
    std::cout << std::endl;

    for (int i = 0; i < size; ++i)
        if (readBack[i] != ((float)id * (float)i * 2.0f)) throw AFK_Exception("Failed!");

    std::cout << "AFK_Computer: Test program completed successfully" << std::endl;

    delete[] readBack;

    clReleaseMemObject(res_b);
    clReleaseMemObject(src_b_b);
    clReleaseMemObject(src_a_b);

    delete[] res_h;
    delete[] src_b_h;
    delete[] src_a_h;
}

bool testComputeWorker(unsigned int id, const struct testComputeParam& param, AFK_WorkQueue<struct testComputeParam, bool>& queue)
{
    bool success = false;
    try
    {
        testComputeInternal(param.computer, param.kernel, id);
        success = true;
    }
    catch (AFK_Exception)
    {
        std::cout << "F";
    }

    return success;
}

void afk_testComputeLong(AFK_Computer *computer, unsigned int concurrency)
{
    AFK_AsyncGang<struct testComputeParam, bool> testComputeGang(
        10, concurrency);
    
    cl_kernel testKernel;
    if (!computer->findKernel("vector_add_gpu", testKernel))
        throw AFK_Exception("Cannot find vector_add_gpu test kernel");

    for (unsigned int i = 0; i < (concurrency * 100); ++i)
    {
        struct testComputeParam p;
        p.computer = computer;
        p.kernel = testKernel;

        AFK_WorkQueue<struct testComputeParam, bool>::WorkItem workItem;
        workItem.func = testComputeWorker;
        workItem.param = p;

        testComputeGang << workItem;
    }

    boost::unique_future<bool> finished = testComputeGang.start();
    finished.wait();
    std::cout << "Finished with " << finished.get() << std::endl;
}

