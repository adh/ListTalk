project('ListTalk', 'c',
        default_options : ['c_std=c11'])

bdwgc = dependency('bdw-gc')

vm_lib = library(
        'ListTalkVM', 
        [
                'src/vm/eval.c',
                'src/vm/value.c',
                'src/vm/Class.c',
                'src/vm/Environment.c',
                'src/vm/base_env/base_env.c',
                'src/vm/base_env/numbers.c',
                'src/vm/base_env/primitives.c',
                'src/vm/base_env/lists.c',
                'src/vm/base_env/strings.c',
                'src/vm/base_env/vectors.c',
                'src/vm/base_env/special_forms.c',
                'src/vm/utils.c',
                'src/vm/init.c',
                'src/vm/error.c',
                'src/vm/throw_catch.c',
                'src/classes/Object.c',
                'src/classes/Boolean.c',
                'src/classes/Printer.c',
                'src/classes/Reader.c',
                'src/classes/Package.c',
                'src/classes/Symbol.c',
                'src/classes/Number.c',
                'src/classes/Float.c',
                'src/classes/String.c',
                'src/classes/Vector.c',
                'src/classes/Closure.c',
                'src/classes/Primitive.c',
                'src/classes/Macro.c',
                'src/classes/SpecialForm.c',
                'src/classes/Nil.c',
                'src/classes/Pair.c',
                'src/classes/SmallInteger.c'
        ], 
        install : true, dependencies : [bdwgc])

executable(
        'listtalk', 
        'src/bin/listtalk/main.c', 
        link_with: [vm_lib], 
        dependencies : [bdwgc], 
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

install_subdir(
        'ListTalk',
        install_dir: get_option('includedir')
)
