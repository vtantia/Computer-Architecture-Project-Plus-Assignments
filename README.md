# CS422 Project

*Note*: Does not compile if a simple make is done, due to macros
which need to be replaced by the python script `runner.py`.

Place this whole folder in ~/pin-3.0-76991-gcc-linux/source/tools/Project

The provided script `runner.py` lets you run the benchmarks easily with clearly
defined logging preferences. Select whichever benchmarks need to be run (`progs` array),
and then select the parameters to be used (the `defines` array).

Once the choices have been selected, the code can be run by simply using:
`python runner.py`.

The outputs will be placed in the benchmark's own folder. For instance, to see
the results on perlbench, navigate to ~/spec_2006/400.perlbench/

# Requirements:
* tmux
* pintool
* Spec 2006 HW2 benchmarks placed in ~/spec_2006/
