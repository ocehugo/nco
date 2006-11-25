Name:           nco
Version:        3.1.5
Release:        3%{?dist}
Summary:        Suite of programs for manipulating NetCDF/HDF4 files
Group:          Applications/Engineering
License:        GPL
URL:            http://nco.sourceforge.net/

#  The NCO web site now recommends CVS so the tar.gz was obtained
#  using the following recommended commands:
#    cvs -d:pserver:anonymous@nco.cvs.sourceforge.net:/cvsroot/nco login
#    cvs -z3 -d:pserver:anonymous@nco.cvs.sourceforge.net:/cvsroot/nco \
#        co -r nco-3_1_5 -d nco-%{version} nco
#    tar -czf nco-%{version}.tar.gz --exclude='nco-3.1.5/debian*' --exclude='.cvsignore' --exclude=ncap_lex.c --exclude='ncap_yacc.[ch]' ./nco-%{version}
Source0:        nco-%{version}.tar.gz
Patch0:		nco_install_C_headers.patch
Patch1:         nco_find_udunits-dat.patch
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  bison, flex
BuildRequires:  netcdf-devel, libtool, automake, autoconf
#  BuildRequires:  udunits, udunits-devel, opendap-devel
BuildRequires:  udunits, udunits-devel
BuildRequires:  curl-devel, libxml2-devel, librx-devel
#  Unfortunately, opendap does not build on FC-4 or later.  When 
#  its fixed, opendap-devel will be listed as a BuildRequires.
#  In the mean time, nco will be built without opendap support.

%package devel
Summary:        Development files for NCO
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description
The netCDF Operators, NCO, are a suite of command line programs known
as operators.  The operators facilitate manipulation and analysis of
self-describing data stored in the freely available netCDF and HDF
formats (http://www.unidata.ucar.edu/packages/netcdf and
http://hdf.ncsa.uiuc.edu, respectively).  Each NCO operator (e.g.,
ncks) takes netCDF or HDF input file(s), performs an operation (e.g.,
averaging, hyperslabbing, or renaming), and outputs a processed netCDF
file.  Although most users of netCDF and HDF data are involved in
scientific research, these data formats, and thus NCO, are generic and
are equally useful in fields from agriculture to zoology.  The NCO
User's Guide illustrates NCO use with examples from the field of
climate modeling and analysis.  The NCO homepage is
http://nco.sourceforge.net/.

%description devel
This package contains the NCO header files and static libs.

%prep
%setup -q
%patch0 -p1
%patch1 -p1

%build
aclocal
autoheader
automake --foreign
autoconf
export CPPFLAGS=-I%{_includedir}/netcdf-3
export LDFLAGS=-L%{_libdir}/netcdf-3
export CFLAGS="$RPM_OPT_FLAGS -fPIC"
export CXXFLAGS="$RPM_OPT_FLAGS -fpermissive -fPIC"
%configure --includedir=%{_includedir}/nco
make %{?_smp_mflags}
unset CPPFLAGS LDFLAGS CFLAGS CXXFLAGS

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}%{_includedir}/nco
make install DESTDIR=${RPM_BUILD_ROOT}
rm -f ${RPM_BUILD_ROOT}%{_libdir}/*.la
rm -f ${RPM_BUILD_ROOT}%{_infodir}/dir
rm -f ${RPM_BUILD_ROOT}%{_bindir}/mpnc*

%clean
rm -rf ${RPM_BUILD_ROOT}


%post
/sbin/ldconfig
/sbin/install-info %{_infodir}/nco.info.gz \
    %{_infodir}/dir 2>/dev/null || :

%postun
/sbin/ldconfig
if [ "$1" = 0 ]; then
  /sbin/install-info --delete %{_infodir}/nco.info.gz \
    %{_infodir}/dir 2>/dev/null || :
fi

%files
%defattr(-,root,root,-)
%doc doc/README doc/LICENSE doc/rtfm.txt
%{_bindir}/*
%{_mandir}/*/*
%{_infodir}/*
%{_libdir}/libnco*[0-9]*.so

%files devel
%defattr(-,root,root,-)
%{_includedir}/nco
%{_libdir}/libnco*.a
%{_libdir}/libnco.so
%{_libdir}/libnco_c++.so


%changelog
* Sat Sep  2 2006 Ed Hill <ed@eh3.com> - 3.1.5-3
- br bison as well

* Sat Sep  2 2006 Ed Hill <ed@eh3.com> - 3.1.5-2
- buildrequire flex

* Sat Sep  2 2006 Ed Hill <ed@eh3.com> - 3.1.5-1
- new upstream 3.1.5

* Fri Apr 21 2006 Ed Hill <ed@eh3.com> - 3.1.2-1
- update to new upstream 3.1.2

* Thu Feb 16 2006 Ed Hill <ed@eh3.com> - 3.0.2-2
- rebuild for new gcc

* Mon Sep  5 2005 Ed Hill <ed@eh3.com> - 3.0.2-1
- update to new upstream 3.0.2

* Wed Aug  3 2005 Ed Hill <ed@eh3.com> - 3.0.1-4
- remove (hopefully only temporarily) opendap support

* Thu Jul 21 2005 Ed Hill <ed@eh3.com> - 3.0.1-3
- add LICENSE file

* Sat Jul  9 2005 Ed Hill <ed@eh3.com> - 3.0.1-2
- add BuildRequires: opendap-devel

* Sun Jun 19 2005 Ed Hill <ed@eh3.com> - 3.0.1-1
- update to upstream 3.0.1
- comment & fixes for BuildRequires

* Sat Apr 23 2005 Ed Hill <ed@eh3.com> - 3.0.0-2
- add BuildRequires and fix CXXFLAGS per Tom Callaway
- add udunits patch per Tom Callaway

* Sat Apr 16 2005 Ed Hill <ed@eh3.com> - 3.0.0-1
- update to ver 3.0.0
- devel package fixes per D.M. Kaplan and M. Schwendt
- fix info post/postun

* Sun Dec  5 2004 Ed Hill <eh3@mit.edu> - 0:2.9.9-0.fdr.4
- sync with netcdf-3.6.0beta6-0.fdr.0
- split into devel and non-devel

* Wed Dec  1 2004 Ed Hill <eh3@mit.edu> - 0:2.9.9-0.fdr.3
- sync with netcdf-0:3.5.1-0.fdr.11
- added '-fpermissive' for GCC 3.4.2 warnings
- added "Provides:nco-devel" for the headers and libs

* Mon Oct  4 2004 Ed Hill <eh3@mit.edu> - 0:2.9.9-0.fdr.2
- Add some of Michael Schwendt's suggested INC/LIB path fixes and 
  sync with the netcdf-3.5.1-0.fdr.10 dependency.

* Thu Sep 23 2004 Ed Hill <eh3@mit.edu> - 0:2.9.9-0.fdr.1
- add NETCDF_INC and NETCDF_LIB to work on systems where old
  versions of netcdf may exist in /usr/local

* Wed Sep  8 2004 Ed Hill <eh3@mit.edu> - 0:2.9.9-0.fdr.0
- updated to ver 2.9.9

* Sat Aug  7 2004 Ed Hill <eh3@mit.edu> - 0:2.9.8-0.fdr.0
- updated to ver 2.9.8

* Sat Jul 17 2004 Ed Hill <eh3@mit.edu> - 0:2.9.7-0.fdr.2
- removed unneeded %ifarch

* Sat Jul 17 2004 Ed Hill <eh3@mit.edu> - 0:2.9.7-0.fdr.1
- Add %post,%postun

* Sat Jul 17 2004 Ed Hill <eh3@mit.edu> - 0:2.9.7-0.fdr.0
- Initial working version

