
require 'mkmf'

$LIBS << ' -lrpcsvc'
$CFLAGS << ' -std=c11'

create_makefile 'rusers/rusers'
