SUBDIRS = include base src

EXTRA_DIST = \
	admin/RPM/aeonwave.spec.in \
	admin/DEBIAN/control.in

deb: dist
	oldpwd=`pwd` ; \
	tmp_dir=`mktemp -d /tmp/$(PACKAGE_NAME).XXXXXXXX` ; \
	cp $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz $${tmpdir}/$(PACKAGE_NAME)_$(PACKAGE_VERSION).orig.tar.gz \
	tar -C $${tmp_dir} -xzf $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz ; \
	mv $${tmp_dir}/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/pkg/debian $${tmp_dir}/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/debian ; \
	cd $${tmp_dir}/$(PACKAGE_NAME)-$(PACKAGE_VERSION) ; \
	pdebuild --buildresult $$oldpwd/.. ; \
	cd - ;\
	rm -rf $${tmp_dir}


