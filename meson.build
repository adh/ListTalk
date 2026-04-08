project('ListTalk', 'c',
        default_options : ['c_std=c11'])

bdwgc = dependency('bdw-gc')
python3 = find_program('python3')
cc = meson.get_compiler('c')
libm = cc.find_library('m', required : false)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')

listtalk_deps = [bdwgc]
listtalk_c_args = []
libedit_dep = dependency('libedit', required : false)

if libedit_dep.found()
    if cc.has_header('editline/readline.h', dependencies : libedit_dep)
        listtalk_deps += [libedit_dep]
        listtalk_c_args += ['-DLT_HAVE_LIBEDIT=1', '-DLT_HAVE_EDITLINE_READLINE_H=1']
    elif cc.has_header('readline/readline.h', dependencies : libedit_dep)
        listtalk_deps += [libedit_dep]
        listtalk_c_args += ['-DLT_HAVE_LIBEDIT=1']
    endif
endif

runtime_init_embedded = custom_target(
        'runtime_init_embedded.c',
        input : 'runtime/init.lt',
        output : 'runtime_init_embedded.c',
        command : [
                python3,
                files('tools/embed_file_as_c.py'),
                '@INPUT@',
                'LT_runtime_init_source',
                '-o',
                '@OUTPUT@'
        ],
        depend_files : files('tools/embed_file_as_c.py')
)

vm_lib = library(
                'ListTalkVM', 
        [
                runtime_init_embedded,
                'src/vm/eval.c',
                'src/vm/quasiquote.c',
                'src/vm/compiler.c',
                'src/vm/value.c',
                'src/vm/Class.c',
                'src/vm/Environment.c',
                'src/vm/base_env/base_env.c',
                'src/vm/base_env/numbers.c',
                'src/vm/base_env/primitives.c',
                'src/vm/base_env/lists.c',
                'src/vm/base_env/cxxxr.c',
                'src/vm/base_env/strings.c',
                'src/vm/base_env/vectors.c',
                'src/vm/base_env/special_forms.c',
                'src/vm/utils.c',
                'src/vm/init.c',
                'src/vm/error.c',
                'src/vm/stack_trace.c',
                'src/vm/conditions.c',
                'src/vm/throw_catch.c',
                'src/classes/Object.c',
                'src/classes/Boolean.c',
                'src/classes/Character.c',
                'src/classes/Printer.c',
                'src/classes/Reader.c',
                'src/classes/Package.c',
                'src/classes/Symbol.c',
                'src/classes/Number.c',
                'src/classes/ComplexNumber.c',
                'src/classes/RealNumber.c',
                'src/classes/RationalNumber.c',
                'src/classes/Integer.c',
                'src/classes/InexactComplexNumber.c',
                'src/classes/ExactComplexNumber.c',
                'src/classes/BigInteger.c',
                'src/classes/Fraction.c',
                'src/classes/Float.c',
                'src/classes/List.c',
                'src/classes/String.c',
                'src/classes/Vector.c',
                'src/classes/Condition.c',
                'src/classes/InvocationContextKind.c',
                'src/classes/Closure.c',
                'src/classes/Primitive.c',
                'src/classes/Macro.c',
                'src/classes/SpecialForm.c',
                'src/classes/Nil.c',
                'src/classes/ImmutableList.c',
                'src/classes/Pair.c',
                'src/classes/StackFrame.c',
                'src/classes/SmallInteger.c',
                'src/classes/SmallFraction.c',
                'src/classes/IdentityDictionary.c',
                'src/classes/Dictionary.c'
        ], 
        install : true, dependencies : [bdwgc, libm])

executable(
        'listtalk', 
        'src/bin/listtalk/main.c', 
        link_with: [vm_lib], 
        dependencies : listtalk_deps,
        c_args : listtalk_c_args,
        install : true
)

reader_test_exe = executable(
        'reader_test',
        'tests/reader_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

eval_test_exe = executable(
        'eval_test',
        'tests/eval_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

throw_catch_test_exe = executable(
        'throw_catch_test',
        'tests/throw_catch_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

conditions_test_exe = executable(
        'conditions_test',
        'tests/conditions_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

identity_dictionary_test_exe = executable(
        'identity_dictionary_test',
        'tests/identity_dictionary_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

dictionary_test_exe = executable(
        'dictionary_test',
        'tests/dictionary_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

test(
        'reader_test',
        reader_test_exe
)

test(
        'eval_test',
        eval_test_exe
)

test(
        'throw_catch_test',
        throw_catch_test_exe
)

test(
        'conditions_test',
        conditions_test_exe
)

test(
        'identity_dictionary_test',
        identity_dictionary_test_exe
)

test(
        'dictionary_test',
        dictionary_test_exe
)

install_subdir(
        'ListTalk',
        install_dir: get_option('includedir')
)
