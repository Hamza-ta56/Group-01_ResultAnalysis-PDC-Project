#!/usr/bin/env bash
# scripts/setup_colab.sh - install the toolchain in a Colab / Ubuntu container.
set -euo pipefail

echo "Updating package index..."
apt-get update -qq

echo "Installing build tools, OpenMP and Open MPI..."
apt-get install -y -qq build-essential openmpi-bin libopenmpi-dev python3 python3-matplotlib

echo
echo "Versions:"
gcc --version | head -n 1
mpicc --version | head -n 1
mpirun --version | head -n 1
python3 -c "import matplotlib; print('matplotlib', matplotlib.__version__)"

echo
echo "Done. Next:"
echo "  make all"
echo "  make datasets"
echo "  make correctness"
