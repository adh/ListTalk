project('ListTalk', 'c',
        default_options : ['c_std=c11'])

bdwgc = dependency('bdw-gc')

vm_lib = library(
        'ListTalkVM', 
        [
                'vm/eval.c',
                'vm/OOP.c',
                'vm/NativeClass.c',
                'vm/utils.c',
                'vm/init.c',
                'vm/Reader.c',
                'vm/Printer.c',
                'vm/Symbol.c',
                'vm/Array.c',
        ], 
        install : true, dependencies : [bdwgc])

executable(
        'listtalk', 
        'tools/repl.c', 
        link_with: [vm_lib], 
        dependencies : [bdwgc], 
        install : true
)

install_headers(
        'ListTalk/ListTalk.h', 
        'ListTalk/OOP.h', 
        subdir: 'ListTalk'
)