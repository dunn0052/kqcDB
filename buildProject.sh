if [[ "$*" == *"-d"* ]]
  then
    ./generateMakeFiles.sh -d
else
    ./generateMakeFiles.sh
fi

cmake --build . -j 12