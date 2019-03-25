# NAME

textFeats - A tool that extracts text features for given Page XMLs or images.

# INSTALLATION AND USAGE

    git clone --recursive https://github.com/mauvilsa/textFeats
    mkdir textFeats/build
    cd textFeats/build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=$HOME ..
    make install
    
    textFeats --help

# CONTRIBUTING

If you intend to contribute, before any commits be sure to first execute githook-pre-commit to setup (symlink) the pre-commit hook. This hook takes care of automatically updating the tool and files versions.

# COPYRIGHT

The MIT License (MIT)

Copyright (c) 2015-present, Mauricio Villegas <mauricio_ville@yahoo.com>
