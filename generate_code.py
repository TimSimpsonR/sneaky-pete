"""
Makes sure the annoying strings and integers Sneaky Pete and Trove use to
communicate stay the same by generating the relevant code for Sneaky Pete.

To do this it takes the path to the .h and .cc files used for Sneaky Pete's
DatastoreStatus class. There it expects to find a pair of magical string
values which it dumps the generated source between. They are:

    //! BEGIN GENERATED CODE
    //! END GENERATED CODE

They only take affect if nothing else outside of spaces appear on the same line.
"""

import argparse
import sys
from trove.common.instance import ServiceStatus
from trove.common.instance import ServiceStatuses


def get_services_enumeration_code():
    names = [name for name in dir(ServiceStatuses)
             if not name.startswith("__")]
    h = []
    cc = []
    for index, name in enumerate(names):
        status = getattr(ServiceStatuses, name)
        comma = "," if index < len(names) - 1 else ""
        h.append('                %s = 0x%x%s  // %s\n'
                 % (name, status.code, comma, status.description))
        cc.append('        case %s:\n' % name)
        cc.append('            return "%s";\n'
                  % status.description.replace('"', '\\"'))
    return h, cc


def splice_lines(original, additional):
    new = []
    replay = True
    for line in original:
        if line.strip() == "//! END GENERATED CODE":
            replay = True
        if replay:
            new.append(line)
        if line.strip() == "//! BEGIN GENERATED CODE":
            replay = False
            new += additional

    return new


def get_sources(args):
    """
    Returns the old source code and the new.
    """
    with open(args.header_file) as file:
        h = file.readlines()
    with open(args.cc_file) as file:
        cc = file.readlines()

    middle_h, middle_cc = get_services_enumeration_code()

    new_h = splice_lines(h, middle_h)
    new_cc = splice_lines(cc, middle_cc)
    return h, cc, new_h, new_cc


def lines_are_different(file_type, a, b):
    for index, line in enumerate(a):
        if line != b[index]:
            print("The generated %s file is different." % file_type)
            print(line)
            print("\tvs")
            print(b[index])
            return True
    if (len(a) != len(b)):
        print("The generate %s file has a different line count." % file_type)
        return True
    return False


def main(args):
    if not (args.printh or args.printcc or args.replace or args.compare):
        print("Specify --printh, --printcc, or --replace.")
        return 1
    old_h, old_cc, new_h, new_cc = get_sources(args)
    h = "".join(new_h)
    cc = "".join(new_cc)

    if args.printh:
        print(h)
    if args.printcc:
        print(cc)
    if args.compare:
        if args.replace:
            print("--compare and --replace cannot be used together.")
            return 1
        if lines_are_different("h", old_h, new_h):
            return 1
        elif lines_are_different("cc", old_cc, new_cc):
            return 1
        else:
            print("No new changes.")
            return 0
    elif args.replace:
        with open(args.header_file, 'w') as h_file:
            h_file.write(h)
        with open(args.cc_file, 'w') as cc_file:
            cc_file.write(cc)
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('header_file', help='Header file to generate.')
    parser.add_argument('cc_file', help='CC file to generate.')
    parser.add_argument('--printh', help='Show the header file.',
                        default=False, action='store_true')
    parser.add_argument('--printcc', help='Show the cc file.',
                        default=False, action='store_true')
    parser.add_argument('--replace', help='Replace existing files.',
                        default=False, action='store_true')
    parser.add_argument('--compare', help='Check to see if the new version of '
                        'the files would be different than the current ones. '
                        'Returns zero if the files are identical and non-zero '
                        'otherwise. Intended to be used by CI.',
                        default=False, action='store_true')
    sys.exit(main(parser.parse_args()))
