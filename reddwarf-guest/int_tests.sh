set -e
bjam nova_guest_mysql nova_guest_apt
PYTHONPATH=bin/gcc-4.4.3/debug
export PYTHONPATH
PATH=$PATH:bin/gcc-4.4.3/debug python "integration/int_tests.py" $@
