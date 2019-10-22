#!/usr/bin/env python3
import sys
import subprocess


def test(exec_name, nx, ny, niter):
    comp = subprocess.run(["./{}".format(exec_name), str(nx), str(ny), str(niter)], capture_output=True)
    a = comp.stdout.decode('latin1')
    i1 = a.find("runtime: ") + len("runtime: ")
    i2 = a.find(" s", i1)
    return float(a[i1:i2])


def verify(ref_name):
    with open(ref_name, "rb") as stencil_ref:
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



def average_out_of(exec_name, nx, ny, niter, times):
    res = []
    for i in range(times):
        res.append(test(exec_name, nx, ny, niter))
    return sum(res)/len(res)


subprocess.run(["make"])

if "full" in sys.argv:
    sizes = [1024, 4096, 8000]
    execs = ["./out/original", "./out/o1","./out/o2",  "./out/o3", "./out/ofast",
                      "./out/clangoriginal", "./out/clango1","./out/clango2",  "./out/clango3", "./out/clangofast" ]
elif "small" in sys.argv:
    sizes = [1024]
elif "big" in sys.argv:
    sizes = [8000]
if "clangofast" in sys.argv:
    execs = ["./out/clangofast"]

for size in sizes:
    for exec_name in execs:
        print("{:20} {} {} 100, average out of 100: ".format(exec_name, size, size)
                ,average_out_of(exec_name, size, size, 100, 100))

#print(verify("stencil_1024_1024_100.pgm"))


exec_name = "./out/iccofast"
for size in range(64, 32064, 128):
        print("{:5}x{:5} average out of 10".format(size, size),
                    average_out_of(exec_name, size, size, 100, 10))
