int main(int argc, char** argv) {
  #ifdef _MSC_VER
  #error
  #endif
  if (argc == 1) {
    return 0;
  } else {
    return argc * 10;
  }
}
