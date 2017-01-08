#!/bin/bash
astyle  --suffix=none --style=linux --indent=spaces=4 --indent-col1-comments --pad-oper --pad-header \
        --unpad-paren --indent-cases --add-brackets --convert-tabs --mode=c --max-code-length=80 --lineend=linux $@
