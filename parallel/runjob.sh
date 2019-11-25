#!/usr/bin/env bash

mkdir -p results

cp stencil.job.temp stencil.job
name="results/stencil.$(date +%Y-%m-%d-%H-%M-%S).out"
sed -i "s,SBATCH --output name.out,SBATCH --output ${name},g" stencil.job
sbatch stencil.job
rm stencil.job

while [ -z "$(grep 'done benching' $name 2>/dev/null)" ]; do
    sleep 1
done

echo "done"

cat $name
#tail -f "$name"
