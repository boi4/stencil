#!/usr/bin/env bash

mkdir -p results

cp stencil.job.temp stencil.job
name="results/stencil.$(date +%Y-%m-%d-%H-%M-%S).out"
sed -i "s,SBATCH --output name.out,SBATCH --output ${name},g" stencil.job
if [[ "$(hostname)" == bc4* ]];then
    . env.sh
    make
    sbatch stencil.job
    echo pwd: $PWD
    echo $name
    #while [ -z "$(grep 'benching' $name 2>/dev/null)" ]; do
    while [ -z "$(grep 'benching' $name)" ]; do
        sleep 10
        squeue -u $USER
        grep -i 'benching' $name
        echo $name
        ls results
    done

    echo "done"

    cat $name
    #tail -f "$name"
else
    if [ -n "$(nmcli connection show --active | grep eduroam)" ];then
        remote="bc4"
    else
        remote="bc4proxy"
    fi
    echo "using remote $remote"
    rsync -avz . ${remote}:working/parallel
    ssh "${remote}" -t "bash -l -c 'cd working/parallel && bash ./runjob.sh'"

    # mkdir -p results
    # rsync -avz "${remote}:working/parallel" .
fi
rm stencil.job
