@echo off
REM ============================================================================
REM  package.bat — Empaqueta Eclips en un paquete autocontenido y distribuible.
REM ============================================================================
REM  Uso:   package.bat [NombreDelJuego]
REM  Desatendido (CI, background, sin consola):  set ECLIPS_SIN_PAUSA=1
REM     Sin eso el `pause` del final espera una tecla PARA SIEMPRE, y desde afuera
REM     se ve identico a un build lento: el paquete ya esta listo y el proceso
REM     sigue vivo con ~0 s de CPU. Cobrado el 2026-09-06 (aceptacion de CP-R4):
REM     una hora colgado con el trabajo terminado desde el minuto cinco.
REM  Ej:    package.bat "Metaverse IDE"
REM  Sin nombre, usa "EclipsJuego".
REM
REM  Genera  out/package/<Nombre>/  con exe + SDL2.dll + assets + manifiesto +
REM  tema + scripts. El doble-clic ya funciona: el motor ancla su CWD al
REM  detectar el manifiesto al lado (A2).
REM ============================================================================

setlocal
set "NOMBRE=%~1"
if "%NOMBRE%"=="" set "NOMBRE=EclipsJuego"

set "PRESET=windows-export"
set "BUILD_DIR=out/build/%PRESET%"
set "DEST=out/package/%NOMBRE%"

echo.
echo === [1/3] Configurando (%PRESET%) ===
cmake --preset %PRESET%
if errorlevel 1 goto :error

echo.
echo === [2/3] Compilando (Release) ===
cmake --build --preset %PRESET%
if errorlevel 1 goto :error

echo.
echo === [3/3] Instalando paquete en %DEST% ===
cmake --install %BUILD_DIR% --prefix "%DEST%" --config Release
if errorlevel 1 goto :error

echo.
echo  OK  Paquete listo:  %DEST%
echo      Proba el standalone con doble-clic en  %DEST%\Eclips.exe
goto :end

:error
echo.
echo  ERROR  Fallo el empaquetado. Revisa los mensajes de arriba.
echo      (sin consola? defini ECLIPS_SIN_PAUSA=1 y este script no espera tecla)
if not defined ECLIPS_SIN_PAUSA if not defined CI pause
endlocal
exit /b 1

:end
echo.
echo      (sin consola? defini ECLIPS_SIN_PAUSA=1 y este script no espera tecla)
if not defined ECLIPS_SIN_PAUSA if not defined CI pause
endlocal