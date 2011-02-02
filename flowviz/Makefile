all: flowviz.jar
SRCS = src/cgi-bin/flowviz.cgi \
	src/pages/flowviz.xml \
	src/pages/configure.xml \
	src/pages/help.xml \
	src/META-INF/plugin.xml \
	src/META-INF/project.xml \
	src/htdocs/configure.php

flowviz.jar: $(SRCS)
	@rm -f $@
	@cd src && zip -r ../$@ *

clean:
	rm -f flowviz.jar

