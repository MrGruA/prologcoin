@ECHO off
SET ROOT=..\..
SET SUBDIR=node
SET DOLIB=node
SET DOEXE=
SET DEPENDS=secp256k1 common interp global ec coin
CALL ..\..\env\make.bat %*

