# data_proxy
# 第一次编译打开Build_Third_Party 编译第三方库 之后关闭该选项
# third_party/lib 加入到动态链接库搜索目录
# build
```sh
    mkdir build
    cd build
    cmake ..
    make -j8
    export LD_LIBRARY_PATH=third_party/lib/:$LD_LIBRARY_PATH
    sudo ldconfig
    ./data_proxy
```