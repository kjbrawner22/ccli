BUILD_TYPE=Debug
TARGET=all
TEST_FLAG=OFF
BUILD_ARGS=""

while getopts 'e:b:itD:G:h' opt; do
  case "$opt" in
  e)
    arg="$OPTARG"
    echo "Target: '${arg}'"
    TARGET=$arg
    ;;
  b)
    arg="$OPTARG"
    echo "Build type: '${arg}'"
    BUILD_TYPE=$arg
    ;;

  i)
    TARGET=install
    echo "Installing project"
    ;;
  
  t)
    TEST_FLAG=ON
    echo "Enabling test compilation"
    ;;
  
  D)
    BUILD_ARGS="$BUILD_ARGS -D$OPTARG"
    ;;

  ? | h)
    echo "Usage: $(basename $0) [-e target][-b Debug|Release] [-i] [-t] [-D CMAKE_FLAG=VALUE]"
    exit 1
    ;;
  esac
done
shift "$(($OPTIND - 1))"

if [[ "$TARGET" == "install" ]] ; then
  TEST_FLAG=OFF
fi

SECONDS=0

cmake -S . -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DENABLE_TESTS=$TEST_FLAG $BUILD_ARGS
cmake --build build --target $TARGET --parallel

echo "Build finished in $SECONDS seconds."
