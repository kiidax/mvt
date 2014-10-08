#!/usr/bin/python

import re

wcswidth = [-1] * 0x10000
for code in range(0, 0xffff):
	if code >= 0x3400 and code <= 0x4dbf or \
		code >= 0x4e00 and code <= 0x9fff or \
		code >= 0xf900 and code <= 0xfaff:
		default = 2
	else:
		default = 1
	wcswidth[code] = default

pattern = r'^([0-9a-fA-F]+);(\S)'
f = open("EastAsianWidth.txt", "r")
for line in f:
	m = re.match(pattern, line)
	if m:
		code = int(m.group(1), 16)
		if code >= 0x10000:
			continue
		ch = m.group(2)
		if ch == 'F' or ch == 'W':
			actual = 2
		else:
			actual = 1
		wcswidth[code] = actual

default = 1
for code in range(0, 0xffff):
	actual = wcswidth[code]
	if actual != default:
		print "    } else if (wc < 0x%04x) {" % (code)
		print "        return %d;" % (default)
		default = actual

#for hi in range(0, 0xff):
#	lone = False
#	first = wcswidth[hi * 0x100]
#	for lo in range(0, 0xff):
#		code = hi * 0x100 + lo
#		if wcswidth[code] != first:
#			lone = True
#			break
#	if lone:
#		print "%02x00:" % (hi)
#		for lo in range(0, 0xff):
#			code = hi * 0x100 + lo
#			actual = wcswidth[code]
#			if actual != -1:
#				print "%04x;%d" % (code, actual)
#	else:
#		if first != -1:
#			print "%02x00: %d" % (hi, first)
