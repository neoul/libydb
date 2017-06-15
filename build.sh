#export LD_LIBRARY_PATH=$PWD/cprops/.libs:$LD_LIBRARY_PATH

echo ${1%.*}
echo $*
gcc $* -lyaml -lcprops -L/home/neoul/projects/c_study/cprops/.libs -I./ -g3 -Wall -o ${1%.*}
