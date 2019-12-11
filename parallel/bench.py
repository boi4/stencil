#!/usr/bin/env python3
import sys
import subprocess

from collections import namedtuple

size = 1024
cpuiter = range(1, 56, 4)
# cpuiter = range(8,9)
# cpuiter = range(56,57)
# cpuiter = range(41,42)
cpuiter = range(1, 4)
# cpuiter = range(5, 6)
# cpuiter = range(15, 16)
# cpuiter = range(16, 17)

config_temp = namedtuple("Config", ["prgname", "verboselevel", "nx", "ny", "niters", "cpuiter"])
config =                config_temp("mpi no stencil", 2, size, size, 100, cpuiter)


## mpirun options:
##        -l or -prepend-rank
##              Use this option to insert the MPI process rank at the beginning of all lines written to the standard output.
##
##       -tune [<arg>]              Use this option to optimize the Intel MPI Library performance by using the data collected by the mpitune utility.
##
##       -trace <profiling library> or -t <profiling library>
##              Use  this  option  to  profile your MPI application with Intel(R) Trace Collector using the indicated profiling_library If you do not specify profiling_library , the default
##              profiling library libVT.so is used.
##
##       -trace-imbalance
##              Use this option to profile your MPI application with Intel Trace Collector using the libVTim.so library.
##
##       -mps   Use this option to collect statistics from your MPI application using MPI Performance Snapshot, which can collect hardware  performance  metrics,  memory  consumption  data,
##              internal MPI imbalance and OpenMP* imbalance statistics. When you use this option, a new folder stat_<date>-<time> is generated that contains the statistics. You can analyze
##              the collected data with the mps utility, which provides a high-level overview of your application performance.  MPI Performance Snapshot is available  as  part  of  Intel(R)
##              Trace Analyzer and Collector.
##
##       -check-mpi [<checking_library>]
##              Use this option to check your MPI application for correctness using the specified checking_library If you do not specify checking_library , the default checking library lib-
##              VTmc.so is used.
##
##       -trace-pt2pt
##              Use this option to collect the information about point-to-point operations using Intel Trace Analyzer and Collector. The option requires that you also use the -trace option.
##
##       -trace-collectives
##              Use this option 



Job = namedtuple("Job", ["number_of_processes", "number_of_threads" ,"command", "args"])



def verify(ref_name):
    with open(ref_name, "rb") as stencil_ref:
        try:
            with open("stencil.pgm", "rb") as stencil_res:
                # Check image header
                ref_format, ref_nx, ref_ny, ref_depth = stencil_ref.readline().split()
                format, nx, ny, depth = stencil_res.readline().split()
                if not (format == ref_format == b"P5"):
                    print("Error: incorrect file format"); sys.exit()
                if not (depth == ref_depth == b"255"):
                    print("Error: incorrect depth value"); sys.exit()
                if not (nx == ref_nx and ny == ref_ny):
                    print("Error: image sizes do not match"); sys.exit()

                # Compare images
                passed = True
                for j in range(int(ref_ny)):
                    for i in range(int(ref_nx)):
                        ref_val = ord(stencil_ref.read(1))
                        val     = ord(stencil_res.read(1))
                        if abs(ref_val - val) > 1:
                            passed = False

                return passed
        except:
            return False


def run_job(job):
    if config.verboselevel:
        print("mpirun -genv OMP_NUM_THREADS {} -np {} {} {}"\
                .format(job.number_of_threads, job.number_of_processes, job.command, " ".join(job.args)))
    comp = subprocess.run(["mpirun",
                            "-genv", "OMP_NUM_THREADS", str(job.number_of_threads),
                           "-np",  str(job.number_of_processes),
                            job.command, *job.args,
                          ],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
    return comp.stdout, comp.stderr



def get_runtime(outtext):
    if type(outtext)==type(b""):
        outtext = outtext.decode("latin1")
    i1 = outtext.find("runtime: ") + len("runtime: ")
    i2 = outtext.find(" s", i1)
    try:
        return float(outtext[i1:i2])
    except:
        return -1.0



def test_cpus_range(outname, nx, ny, niters, cpuiter, ver=False, vername=""):
    threaditer=range(1,2,1) 
    print(nx, ny, niters)
    print("-"*20)
    false_ones = []
    for ncpus in cpuiter:
        for nthreads in threaditer:
            if ver:
                subprocess.run(["/bin/rm",  "stencil.pgm"])
            job = Job(ncpus, nthreads, outname, [str(nx), str(ny), str(niters)])
            stdout, stderr = run_job(job)
            print(ncpus, nthreads, get_runtime(stdout))
            if ver:
                f = verify(vername)
                print("Verify with", vername, ":", f)
                if not f:
                    false_ones.append(ncpus)
            if config.verboselevel == 2:
                print(stdout.decode("utf-8"))
                print("="*20+2*'\n')
    if ver:
        print(false_ones)


print("hello from python")
print(config.prgname)
test_cpus_range("./out/mpistencil", config.nx, config.ny , config.niters, config.cpuiter, True, "clean.pgm")
