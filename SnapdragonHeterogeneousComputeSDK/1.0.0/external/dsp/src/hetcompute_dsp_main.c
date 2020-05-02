#include "AEEStdDef.h"
#include "rpcmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "hetcompute_dsp.h"

int
main(int argc, char* argv[])
{
    printf("\nStarting DSP Test\n");

    int64 result = 0;
    int   vec[5] = { 1, 2, 3, 4, 5 };

    int status = hetcompute_dsp_sum(vec, 5, &result);

    if (status)
    {
        printf("Failure, Status = %d\n\n", status);
    }
    else
    {
        printf("Success, Result = %llu\n\n", result);
    }


    return status;
}
