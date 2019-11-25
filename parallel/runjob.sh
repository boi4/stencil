#!/usr/bin/env bash

mkdir -p results

cp stencil.job.temp stencil.job
name="results/stencil.$(date +%Y-%m-%d-%H-%M-%S)"
sed -i "s,SBATCH --output name.out,SBATCH --output ${name}.out,g" stencil.job
sbatch stencil.job
rm stencil.job

while [ ! -f "$name" ]; do
    sleep 1
done

echo "done"

cat $name
#tail -f "$name"
