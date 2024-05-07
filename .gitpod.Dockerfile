FROM gitpod/workspace-full-vnc

USER gitpod

RUN wget -q -O - https://dl.google.com/linux/linux_signing_key.pub | sudo apt-key add -

RUN sudo apt-get -q update && \
    sudo apt-get -yq upgrade && \
    sudo apt-get -yq install qemu-system-x86 qemu-utils gdb-mingw-w64 && \
    sudo rm -rf /var/lib/apt/lists/*

# RUN wget https://svn.reactos.org/amine/RosBEBinFull.tar.gz && \

RUN wget https://sourceforge.net/projects/reactos/files/RosBE-Unix/2.2.1/RosBE-Unix-2.2.1.tar.bz2/download?use_mirror=unlimited -O RosBE-Unix.tar.bz2 && \
    sudo tar -xvf RosBE-Unix.tar.bz2 -C /usr/local --one-top-level=RosBE --strip-components 1 && \
    rm -f RosBE-Unix.tar.bz2

RUN echo 'export PATH=/usr/local/RosBE/i386/bin:$PATH' >> /home/gitpod/.profile

RUN qemu-img create -f qcow2 reactos_hdd.qcow 10G
