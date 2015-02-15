#!/usr/bin/env python
import sys, urllib2
from lxml import html

authors = set()
for author in sys.stdin:
	author = author.strip()
	if author == '(no author)':
		print author + ' = ' + author
	else:
		try:
			response =  urllib2.urlopen('http://sourceforge.net/users/' + author)
			body = response.read()
			dom = html.fromstring(body)
			# h1.project_title a.project_link
			name = dom.xpath('//h1[@class="project_title"]/a[@class="project_link"]/text()')
			address = name[0] + ' <' + author + '@users.sourceforge.net>'
			print author + ' = ' + address
		except IOError, e:
			print author + " = " + author + " <" + author + "@users.sourceforge.net>"
