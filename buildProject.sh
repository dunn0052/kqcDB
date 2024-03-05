if [[ "$*" == *"-d"* ]]
  then
    ./generateMakeFiles.sh -d
    cmake --build . -j 12
else
    ./generateMakeFiles.sh
    cmake --build . -j 12
fi