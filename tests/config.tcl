#
# nsvfs configuration test.
#


set homedir   [pwd]/tests
set bindir    [file dirname [ns_info nsd]]



#
# Global Naviserver parameters.
#

ns_section "ns/parameters"
ns_param   home           $homedir
ns_param   tcllibrary     $bindir/../tcl
ns_param   logdebug       false

ns_section "ns/modules"
ns_param   nssock         $bindir/nssock.so

ns_section "ns/module/nssock"
ns_param   port            8080
ns_param   hostname        localhost
ns_param   address         127.0.0.1
ns_param   defaultserver   server1

ns_section "ns/module/nssock/servers"
ns_param   server1         server1

ns_section "ns/servers"
ns_param   server1         "Server One"


#
# Server One configuration.
#

ns_section "ns/server/server1/tcl"
ns_param   initfile        ${bindir}/init.tcl
ns_param   library         $homedir/server1/modules

ns_section "ns/server/server1/modules"
ns_param   nsvfs           $homedir/../nsvfs.so


#
# Fastpath/vfs configuration.
#

ns_section "ns/fastpath"

ns_section "ns/server/server1/fastpath"
ns_param   directoryfile     iiindex.html
ns_param   directorylisting  simple
ns_param   serverdir         server1
ns_param   pagedir           pages
