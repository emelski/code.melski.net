all: flowviz.jar

flowviz.jar: src/cgi-bin/flowviz.cgi src/pages/flowviz.xml src/pages/help.xml
	@rm -f $@
	@cd src && zip -r ../$@ *

clean:
	rm -f flowviz.jar

