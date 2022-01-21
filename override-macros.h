
#define __NL__ // For debugging: Comment this line and replace __NL__ with newlines using a text editor

// Select argument name i from the variable argument list (ignoring types)
#define OVERRIDE_ARG(i, ...)  OVERRIDE_ARG_##i(__VA_ARGS__)
#define OVERRIDE_ARG_1(type1, arg1, ...)  arg1
#define OVERRIDE_ARG_2(type1, arg1, type2, arg2, ...)  arg2
#define OVERRIDE_ARG_3(type1, arg1, type2, arg2, type3, arg3, ...)  arg3
#define OVERRIDE_ARG_4(type1, arg1, type2, arg2, type3, arg3, type4, arg4, ...)  arg4
#define OVERRIDE_ARG_5(type1, arg1, type2, arg2, type3, arg3, type4, arg4, arg5, ...)  arg5

// Create the function pointer typedef for the function
#define OVERRIDE_TYPEDEF_NAME(funcname) orig_##funcname##_func_type
#define OVERRIDE_TYPEDEF(has_varargs, nargs, returntype, funcname, ...)  typedef returntype (*OVERRIDE_TYPEDEF_NAME(funcname))(OVERRIDE_ARGS(has_varargs, nargs, __VA_ARGS__));

// Create a valid C argument list including types
#define OVERRIDE_ARGS(has_varargs, nargs, ...)  OVERRIDE_ARGS_##nargs(has_varargs, __VA_ARGS__)
#define OVERRIDE_ARGS_1(has_varargs, type1, arg1)  type1 arg1 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_2(has_varargs, type1, arg1, type2, arg2)  type1 arg1, type2 arg2 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_3(has_varargs, type1, arg1, type2, arg2, type3, arg3)  type1 arg1, type2 arg2, type3 arg3 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_4(has_varargs, type1, arg1, type2, arg2, type3, arg3, type4, arg4)  type1 arg1, type2 arg2, type3 arg3, type4 arg4 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_5(has_varargs, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5 OVERRIDE_VARARGS(has_varargs)
// Print ", ..." in the argument list if has_varargs is 1
#define OVERRIDE_VARARGS(has_varargs) OVERRIDE_VARARGS_##has_varargs
#define OVERRIDE_VARARGS_0
#define OVERRIDE_VARARGS_1 , ...

// Create an argument list without types where one argument is replaced with new_path
#define OVERRIDE_RETURN_ARGS(nargs, path_arg_pos, ...)  OVERRIDE_RETURN_ARGS_##nargs##_##path_arg_pos(__VA_ARGS__)
#define OVERRIDE_RETURN_ARGS_1_1(type1, arg1)  new_path
#define OVERRIDE_RETURN_ARGS_2_1(type1, arg1, type2, arg2)  new_path, arg2
#define OVERRIDE_RETURN_ARGS_2_2(type1, arg1, type2, arg2)  arg1, new_path
#define OVERRIDE_RETURN_ARGS_3_1(type1, arg1, type2, arg2, type3, arg3)  new_path, arg2, arg3
#define OVERRIDE_RETURN_ARGS_3_2(type1, arg1, type2, arg2, type3, arg3)  arg1, new_path, arg3
#define OVERRIDE_RETURN_ARGS_3_3(type1, arg1, type2, arg2, type3, arg3)  arg1, arg2, new_path
#define OVERRIDE_RETURN_ARGS_4_1(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  new_path, arg2, arg3, arg4
#define OVERRIDE_RETURN_ARGS_4_2(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  arg1, new_path, arg3, arg4
#define OVERRIDE_RETURN_ARGS_4_3(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  arg1, arg2, new_path, arg4
#define OVERRIDE_RETURN_ARGS_4_4(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  arg1, arg2, arg3, new_path
#define OVERRIDE_RETURN_ARGS_5_1(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  new_path, arg2, arg3, arg4, arg5
#define OVERRIDE_RETURN_ARGS_5_2(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, new_path, arg3, arg4, arg5
#define OVERRIDE_RETURN_ARGS_5_3(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, arg2, new_path, arg4, arg5
#define OVERRIDE_RETURN_ARGS_5_4(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, arg2, arg3, new_path, arg5
#define OVERRIDE_RETURN_ARGS_5_5(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, arg2, arg3, arg4, new_path

// Use this to override a function without varargs
#define OVERRIDE_FUNCTION(nargs, path_arg_pos, returntype, funcname, ...) \
    OVERRIDE_FUNCTION_MODE_GENERIC(0, nargs, path_arg_pos, returntype, funcname, __VA_ARGS__)

// Use this to override a function with a vararg mode that works like open() or openat()
#define OVERRIDE_FUNCTION_VARARGS(nargs, path_arg_pos, returntype, funcname, ...) \
    OVERRIDE_FUNCTION_MODE_GENERIC(1, nargs, path_arg_pos, returntype, funcname, __VA_ARGS__)

#define OVERRIDE_FUNCTION_MODE_GENERIC(has_varargs, nargs, path_arg_pos, returntype, funcname, ...) \
OVERRIDE_TYPEDEF(has_varargs, nargs, returntype, funcname, __VA_ARGS__) \
__NL__ returntype funcname (OVERRIDE_ARGS(has_varargs, nargs, __VA_ARGS__))\
__NL__{\
__NL__    debug_fprintf(stderr, #funcname "(%s) called\n", OVERRIDE_ARG(path_arg_pos, __VA_ARGS__));\
__NL__    char buffer[MAX_PATH];\
__NL__    const char *new_path = fix_path(OVERRIDE_ARG(path_arg_pos, __VA_ARGS__), buffer, sizeof buffer);\
__NL__ \
__NL__    static OVERRIDE_TYPEDEF_NAME(funcname) orig_func = NULL;\
__NL__    if (orig_func == NULL) {\
__NL__        orig_func = (OVERRIDE_TYPEDEF_NAME(funcname))dlsym(RTLD_NEXT, #funcname);\
__NL__    }\
__NL__    OVERRIDE_DO_MODE_VARARG(has_varargs, nargs, path_arg_pos, __VA_ARGS__) \
__NL__    return orig_func(OVERRIDE_RETURN_ARGS(nargs, path_arg_pos, __VA_ARGS__));\
__NL__}

// Conditionally expands to the code used to handle the mode argument of open() and openat()
#define OVERRIDE_DO_MODE_VARARG(has_mode_vararg, nargs, path_arg_pos, ...) \
    OVERRIDE_DO_MODE_VARARG_##has_mode_vararg(nargs, path_arg_pos, __VA_ARGS__)
#define OVERRIDE_DO_MODE_VARARG_0(nargs, path_arg_pos, ...) // Do nothing
#define OVERRIDE_DO_MODE_VARARG_1(nargs, path_arg_pos, ...) \
__NL__    if (__OPEN_NEEDS_MODE(flags)) {\
__NL__        va_list args;\
__NL__        va_start(args, flags);\
__NL__        int mode = va_arg(args, int);\
__NL__        va_end(args);\
__NL__        return orig_func(OVERRIDE_RETURN_ARGS(nargs, path_arg_pos, __VA_ARGS__), mode);\
__NL__    }
