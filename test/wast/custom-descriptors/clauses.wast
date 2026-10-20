(assert_invalid
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\09\01\4d\ff\ff\ff\ff\0f\5f\00")
  "unknown type")
(assert_invalid
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\09\01\4c\ff\ff\ff\ff\0f\5f\00")
  "unknown type")
