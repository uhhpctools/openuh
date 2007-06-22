#
#  Copyright (C) 2007. QLogic Corporation. All Rights Reserved.
#

#
#
#  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it would be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
#
#  Further, this software is distributed without any warranty that it is
#  free of the rightful claim of any third person regarding infringement 
#  or the like.  Any license provided herein, whether implied or 
#  otherwise, applies only to this software file.  Patent licenses, if 
#  any, provided herein do not apply to combinations of this program with 
#  other software, or any other product whatsoever.  
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, write the Free Software Foundation, Inc., 59
#  Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
#  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
#  Mountain View, CA 94043, or:
#
#  http://www.sgi.com
#
#  For further information regarding this notice, see:
#
#  http://oss.sgi.com/projects/GenInfo/NoticeExplan
#
#

# sort the options list so the read_options can group things properly
BEGIN {
	options = 0
	optfile = "tmp.options"
	comfile = "tmp.options.combo"
	print "" > optfile
	print "" > comfile
}
{
if ($1 == "%%%" && $2 == "OPTIONS") {
	options = 1;
	next;
} else if ($1 == "%%%" && $2 == "COMBINATIONS") {
	options = 0;
	next;
}
if (options) {
	# combine -name lines with help msg, onto one line,
	# so sorting is easier.
	# add | character between implies and help msg.
	firstchar = substr($1,1,1);
	lastchar = substr($1, length($1), 1);
	if (firstchar == "-" || firstchar == "I")
		printf "%s | ", $0 >> optfile;

	# KEY:  Support double-quoting the option name.
	else if (firstchar == "\"" &&
		 lastchar == "\"" &&
		 NF > 1) {		# number of fields
		# Print out first record without double quotes.
		printf "%s", substr($1, 2, length($1) - 2) >> optfile;

		# Print out rest of records.
		for (i = 2; i <= NF; i++) {
		 	printf " %s", $i >> optfile;
		}
		printf " | " >> optfile;
	}

	else if (firstchar == "\"")
		printf "%s\n", $0 >> optfile;
} else {
	print $0 >> comfile;
}
}
