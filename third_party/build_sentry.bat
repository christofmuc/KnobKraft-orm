cd sentry-native
cmake -B build
cmake --build build --parallel --config RelWithDebInfo
cmake --install build --prefix install --config RelWithDebInfo
