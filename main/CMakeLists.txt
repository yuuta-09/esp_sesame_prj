set(srcs "")
file(GLOB_RECURSE all_c_files "./*.c")
list(APPEND srcs "${all_c_files}")

idf_component_register(SRCS "main.c" "blecent.c" "${srcs}"
                      INCLUDE_DIRS "." "sesame" "utils" "firebase" "firebase_sesame")
target_add_binary_data(${COMPONENT_TARGET} "roots.pem" TEXT)
