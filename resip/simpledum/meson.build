
install_headers(['simpledum.h'], subdir : 'simpledum')

libsimpledum_lib = shared_library('simpledum',
    ['simpledum.cxx'],
    include_directories: [include_directories('.')],
    link_with : [libdum_lib],
    install : true,
)

pkgconfig = import('pkgconfig')
pkgconfig.generate(libsimpledum_lib, filebase : 'libsimpledum')