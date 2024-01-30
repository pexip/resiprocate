libresipares_sources = [
  'ares__close_sockets.c',
  'ares__get_hostent.c',
  'ares__read_line.c',
  'ares_destroy.c',
  'ares_expand_name.c',
  'ares_fds.c',
  'ares_free_errmem.c',
  'ares_free_hostent.c',
  'ares_free_string.c',
  'ares_gethostbyaddr.c',
  'ares_gethostbyname.c',
  'ares_init.c',
  'ares_mkquery.c',
  'ares_parse_a_reply.c',
  'ares_parse_ptr_reply.c',
  'ares_process.c',
  'ares_query.c',
  'ares_search.c',
  'ares_send.c',
  'ares_strerror.c',
  'ares_timeout.c',
  'ares_local.c',
]

libresipares_headers = [
  'ares.h',
  'ares_compat.h',
  'ares_dns.h',
  'ares_local.h',
  'ares_socketfunc.h',
  'ares_version.h',
]

libresipares_deps = []

if ['darwin', 'ios'].contains(host_system)
  libresipares_deps += [
    dependency('CoreFoundation', required: true),
    dependency('SystemConfiguration', required: true),
  ]
endif

install_headers(libresipares_headers, subdir : 'rutil/dns/ares')

libresipares_lib = static_library('resipares',
    libresipares_sources,
    dependencies : libresipares_deps,
    include_directories: [base_dir, include_directories('.')],
    install : false,
)
