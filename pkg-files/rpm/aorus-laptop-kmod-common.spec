Name:       aorus-laptop-kmod-common

Version:        0.1.0
Release:        1%{?dist}
Summary:        Common file for aorus-laptop drivers
License:        GPL
URL:            https://github.com/tangalbert919/gigabyte-laptop-wmi

Source0:        aorus-laptop.conf

Requires:       aorus-laptop-kmod = %{?epoch:%{epoch}:}%{version}
Provides:       aorus-laptop-kmod-common = %{?epoch:%{epoch}:}%{version}

%description
Common package for aorus-laptop drivers. Mostly empty.

%install
install -p -m 0644 -D %{SOURCE0} %{buildroot}%{_sysconfdir}/modules-load.d/aorus-laptop.conf

%files
%{_sysconfdir}/modules-load.d/aorus-laptop.conf

%changelog
