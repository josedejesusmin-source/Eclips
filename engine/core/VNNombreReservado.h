// ============================================================================
// ECLIPS ENGINE - VNNombreReservado  (ficha SET-NO-PROTEGE-EL-GUION-BAJO)
// ============================================================================
//
// LA PREGUNTA QUE ESTE HEADER CONTESTA
//
//     set _hora = 5      ->  ¿quien es el dueno de ese nombre?
//
// `meta_set` rechaza todo nombre que empieza con `_` (`VNScriptState.h:12702`,
// *"reservado para variables de sistema"*) y `executeSet` **no rechaza nada**.
// La ficha decia que por eso un `set _hora = 5` es una escritura muerta, y el
// codigo la corrigio: es muerta SOLO si `resolveVariable` contesta ese nombre
// antes del fall-through. Para cualquier otro `_nombre`, el fall-through llega
// a `m_variables` y la variable SI se lee.
//
// O sea que el prefijo `_` no dice nada por si solo. Lo que decide es **si el
// nombre ya tiene dueno**, y hay tres respuestas, no dos.
//
// ----------------------------------------------------------------------------
// ACTUALIZACION (`meta-set-simetrico`, 2026-08-18): YA NO HAY DOS LEYES
// ----------------------------------------------------------------------------
// El parrafo de arriba queda como se escribio, y su diagnostico sigue siendo
// el correcto -- pero la mitad que describe a `meta_set` **ya no es cierta**.
// Cuando se escribio, el motor tenia DOS leyes de nombres:
//
//     `executeSet`      esta ley (tres respuestas, lista enumerada)
//     `executeMetaSet`  "empieza con `_`" -> rechazado, sin mirar cual
//     los 3 verbos Lua meta                 nada, ni una pregunta
//
// El rechazo por prefijo custodiaba *"variables de sistema"* **en un store
// donde no vive ninguna**: el motor escribe UNA sola clave al MetaStore
// (`main.cpp:332` -> `intro_motor_vista`) y no lleva `_`. Y mientras tanto
// `eclips.meta_set/add/sub` escribian cualquier cosa, o sea que la regla del
// `.ecl` era esquivable por la puerta de al lado.
//
// Desde ese lote las CUATRO puertas del MetaStore preguntan **esta** ley. El
// rechazo no desaparecio: cambio de dueno. Dejo de proteger un conjunto vacio
// y pasa a proteger AL LECTOR -- un `_hora` en meta es invisible para `${}` y
// encima permanente. Lo que se relajo es lo que la ficha pedia: un `_nombre`
// del creador ahora puede ser persistente, igual que ya podia ser de partida.
//
// ----------------------------------------------------------------------------
// LAS TRES RESPUESTAS, Y POR QUE NO SE TRATAN IGUAL  (fork 2b, firmado)
// ----------------------------------------------------------------------------
// El criterio que las separa es **quien sufre el error**:
//
//  · `Sombreado`        el creador escribe al vacio y NO SE ENTERA. La lectura
//                       nunca va a llegar a `m_variables` porque
//                       `resolveVariable` contesta antes. -> hay que hablar.
//
//  · `EstadoDelMotor`   el error lo sufre EL MOTOR, y sobrevive al save.
//                       `_tema_activo` no es una escritura muerta: es lo que el
//                       motor lee para decidir si re-aplica el tema
//                       (`VNScriptState.h:1716` y `:1733`) y viaja adentro de
//                       la partida guardada. -> hay que impedirlo.
//
//  · `Libre`            todo lo demas, **incluido `_palabra`, `_palabra_var` y
//                       `_batalla_ganada`**. Ahi el motor PUBLICA un dato PARA
//                       el creador; que el creador lo pise es un efecto visible
//                       y transitorio (la proxima batalla / el proximo pick lo
//                       reescriben), y en `_palabra` el diseno de `CP3` ya lo
//                       trato como caso legitimo -- ver el comentario del alias
//                       `_word` en `resolveVariable`, que existe justamente
//                       para que un `set _palabra = x` del creador no mueva una
//                       grafia y deje la otra atras.
//
// **`_batalla_ganada` NO se nombra en este archivo, y es deliberado.** Su
// origen es un hardcode candidato a poda (HORIZONTES, ficha del `hp:` de
// `iniciar_batalla`) pero la INTENCION de atras -- que el motor publique un
// resultado de gameplay que el VN pueda ramificar -- es la que se queda. Si ese
// canal se poda o se renombra, este header no se entera y no hay que tocarlo.
// Un guardian que lo hubiera listado seria un guardian con fecha de
// vencimiento.
//
// ----------------------------------------------------------------------------
// LA LISTA SE COPIA, Y LA COPIA SE VERIFICA POR MAQUINA
// ----------------------------------------------------------------------------
// La forma linda seria preguntarle a `resolveVariable` en vez de repetir sus
// nombres. **No se puede:** es miembro de `VNScriptState`, que arrastra GL, y
// esta ley tiene que ser SDL-free para que un harness la corra (misma pared que
// `VNCompare.h`).
//
// Asi que la lista de abajo ES una segunda copia, que es exactamente la
// enfermedad de §7.310 -- un inventario a mano contra algo que crece solo. Lo
// que la desarma no es una promesa: es `harness_nombres §2`, que **abre
// `VNScriptState.h` como archivo**, extrae cada `varName == "_..."` del cuerpo
// de `resolveVariable` y exige que los dos conjuntos sean IGUALES.
//
// -> Una variable de sistema nueva que nazca sin entrar aca pone ROJO sola.
// -> Un nombre que se agregue aca sin existir alla, tambien.
//
// Si tocas `resolveVariable`, no hace falta que te acuerdes de este archivo.
// El harness se acuerda.
//
// ----------------------------------------------------------------------------
// LO QUE ESTE HEADER **NO** HACE
// ----------------------------------------------------------------------------
// No escribe, no lee `m_variables`, **no incluye ni toca el MetaStore** y no
// decide que hace el caller con la respuesta. Contesta *"¿de quien es este
// nombre?"* y arma la voz. Rechazar o no es del cable, igual que `VNCompare.h`
// recibe los dos lados ya resueltos y solo pone la ley de encima.
//
// Desde `meta-set-simetrico` **nombra** al MetaStore en un enum y en un texto,
// que no es lo mismo que conocerlo: `AlmacenDeNombre` es una etiqueta que el
// caller trae para que la voz no mienta sobre donde quedo el dato. Este header
// sigue sin incluir `MetaStore.h` y sigue siendo SDL-free -- si un dia hiciera
// falta preguntarle algo al store de verdad, la ley se estaria mudando de piso
// y eso es otra decision, no un parametro mas.
//
// ============================================================================

#pragma once

#include <string>

namespace eclips
{
    // ── Quien es el dueno de un nombre ───────────────────────────────────────
    enum class DuenioDeNombre
    {
        Libre = 0,        // nadie lo reclama -> el creador escribe tranquilo
        Sombreado,        // `resolveVariable` contesta antes -> escritura MUERTA
        EstadoDelMotor    // el motor lo escribe Y lo lee -> la escritura lo CORROMPE
    };

    // ── En que almacen cae la escritura ─────────────────────────────────────
    //
    // EL DUENO DEL NOMBRE NO DEPENDE DEL ALMACEN -- `_hora` es del reloj lo
    // escriba quien lo escriba, y por eso `duenioDeNombre` no recibe esto. Lo
    // que SI depende del almacen es **que le pasa a la escritura equivocada**,
    // y la voz tiene que decir la verdad de SU puerta:
    //
    //  · `Partida`   `m_variables`. Muere con la partida y viaja adentro del
    //                save.
    //
    //  · `Meta`      el MetaStore (`meta.eclips`). NO viaja en el save: es
    //                otro archivo, y sobrevive incluso a borrar los saves.
    //                `resolveVariable` lo consulta DESPUES de `m_variables`, o
    //                sea despues del bloque del `_` -- asi que un sombreado
    //                escrito aca es invisible para `${}` y para `si`, y
    //                ademas es PERMANENTE.
    //
    // Sin este parametro la voz de `meta_set` diria *"ademas viaja adentro de
    // la partida guardada"*, que es lo contrario de lo que ese comando hace --
    // y es justo la distincion por la que `meta_set` existe (*"las variables
    // son del juego, no del save"*). Una voz que miente sobre DONDE quedo el
    // dato es peor que no decir nada, porque manda a buscar el problema al
    // archivo equivocado.
    //
    // VA SIN DEFAULT, por la misma razon que `comando` (§7.534): con default,
    // la puerta que se agregue manana hereda el almacen de la primera y nada
    // se pone rojo. Sin default, agregar una puerta OBLIGA a nombrar su store,
    // y olvidarse no compila.
    enum class AlmacenDeNombre
    {
        Partida = 0,
        Meta
    };

    namespace nombre_detail
    {
        // Los 24 nombres fijos que `resolveVariable` atiende dentro de su
        // bloque del `_`, en el mismo orden en que aparecen alla. El conteo
        // esta sellado en `harness_nombres §3`: si esta tabla crece o se
        // encoge sin que crezca o se encoja `resolveVariable`, se pone rojo.
        //
        // El centinela `nullptr` es lo que permite barrerla sin pasar un largo
        // aparte -- mismo recurso que `EasingId::None` en su tabla de curvas.
        inline const char* const* nombresSombreados()
        {
            static const char* const kTabla[] = {
                "_idioma",     "_language",      // LangStore
                "_saves",      "_partidas",      // VNSaveMenu::countSaves()
                "_word",       "_word_var",      // alias ingles del pick (CP3)
                "_hora",       "_hour",
                "_minuto",     "_minute",
                "_hora_pad",   "_hour_pad",
                "_minuto_pad", "_minute_pad",
                "_dia",        "_day",
                "_mes",        "_month",
                "_anio",       "_year",
                "_dia_semana", "_weekday",
                "_version_motor", "_engine_version",  // Types.h ENGINE_VERSION
                nullptr
            };
            return kTabla;
        }

        // La familia con punto. **El punto es parte del prefijo y no es un
        // detalle:** `InputManager` rechaza `_tecla` (sin punto) y `_teclas`
        // con el mismo `rfind("tecla.", 0)`, asi que esos dos CAEN al
        // fall-through y son `Libre`. Copiar el prefijo sin el punto haria a
        // este guardian mas ancho que la ley que dice custodiar.
        inline bool empiezaCon(const std::string& s, const char* pre)
        {
            return s.rfind(pre, 0) == 0;
        }
    }

    // ── La ley ───────────────────────────────────────────────────────────────
    //
    // Un nombre vacio es `Libre`: no es trabajo de este header decidir que pasa
    // con una linea sin variable -- eso ya lo dice el parser, con su numero de
    // linea, que es donde el aviso sirve.
    inline DuenioDeNombre duenioDeNombre(const std::string& nombre)
    {
        if (nombre.empty() || nombre[0] != '_')
            return DuenioDeNombre::Libre;

        // (b2) El unico estado interno del motor que vive en `m_variables` con
        // clave literal Y que el motor vuelve a leer. Censado: 3 escrituras
        // (`VNScriptState.h:378`, `:9363`, `:9388`) y 3 lecturas internas.
        if (nombre == "_tema_activo")
            return DuenioDeNombre::EstadoDelMotor;

        // (a) La familia con punto, antes que la tabla: no esta en la tabla
        // porque no es un nombre, es un prefijo.
        if (nombre_detail::empiezaCon(nombre, "_tecla.") ||
            nombre_detail::empiezaCon(nombre, "_key."))
            return DuenioDeNombre::Sombreado;

        // (a) Los 24 fijos.
        for (const char* const* p = nombre_detail::nombresSombreados(); *p; ++p)
            if (nombre == *p)
                return DuenioDeNombre::Sombreado;

        // (c) Libre -- y aca caen `_palabra`, `_palabra_var`,
        // `_batalla_ganada`, `_tecla` sin punto, y cualquier `_loquesea` que el
        // creador invente. Los cuatro se leen hoy por el fall-through de
        // `resolveVariable`, asi que rechazarlos seria romper conducta viva.
        return DuenioDeNombre::Libre;
    }

    // ── La voz ───────────────────────────────────────────────────────────────
    //
    // Dos causas distintas piden dos textos distintos, y no por cortesia: el
    // creador tiene que poder saber si su problema es *"esto no se va a leer"*
    // o *"esto le pertenece al motor"*, porque el arreglo no es el mismo. La
    // voz ENSENA la salida, igual que la de `VNCompare`.
    // `comando` NO tiene default, y eso es deliberado. La voz nombra al
    // comando que el creador escribio, asi que un default la haria decir
    // "set:" desde `entrada` o desde `eclips.add` -- mandando a leer un
    // renglon del manual que no es el suyo. Es §7.534 en miniatura: un default
    // silencioso no falla a una constante, falla al UNICO valor que el call
    // site nuevo no queria. Sin default, agregar una puerta obliga a nombrarla.
    //
    // `almacen` entro por la misma puerta, y por una MEDICION y no por
    // simetria: los dos textos de abajo tenian una clausula que solo es cierta
    // en `m_variables`.
    //
    //   · el de `EstadoDelMotor` promete el SAVE -- y `meta_set` no escribe en
    //     el save; escribe en `meta.eclips`, que es la distincion por la que
    //     ese comando existe. Una voz que miente sobre DONDE quedo el dato
    //     manda a buscar el problema al archivo equivocado.
    //
    //   · el de `Sombreado` dice *"no la iba a leer nadie"* -- y en meta SI la
    //     lee `eclips.meta_get`, que va al store directo sin pasar por la
    //     sombra. Lo que no la lee es EclipScript (`${}` y `si`).
    //
    // Son CUATRO textos, no dos. `harness_nombres §10` exige que los cuatro
    // sean distintos entre si y que ninguno hable del almacen del otro.
    inline std::string vozNombreReservado(DuenioDeNombre duenio,
                                          const std::string& nombre,
                                          const std::string& comando,
                                          AlmacenDeNombre almacen)
    {
        if (duenio == DuenioDeNombre::Sombreado)
        {
            if (almacen == AlmacenDeNombre::Meta)
            {
                return "[VNScriptState] " + comando + ": '" + nombre + "' es una variable de "
                       "sistema: el motor la contesta al leerla, asi que '${" + nombre + "}' "
                       "nunca iba a devolver lo que guardaste. Y meta.eclips sobrevive a "
                       "borrar saves, o sea que el valor quedaba ahi para siempre. Se ignoro.\n"
                       "  Si querias tu propia variable persistente, usa otro nombre "
                       "(por ejemplo '" + nombre + "_mio').";
            }
            return "[VNScriptState] " + comando + ": '" + nombre + "' es una variable de "
                   "sistema: el motor la contesta al leerla, asi que esta "
                   "escritura no la iba a leer nadie. Se ignoro.\n"
                   "  Si querias tu propia variable, usa otro nombre "
                   "(por ejemplo '" + nombre + "_mio').";
        }
        if (duenio == DuenioDeNombre::EstadoDelMotor)
        {
            if (almacen == AlmacenDeNombre::Meta)
            {
                return "[VNScriptState] " + comando + ": '" + nombre + "' es estado interno del "
                       "motor, y el motor lo escribe y lo lee en la partida, no en "
                       "meta.eclips. Guardarlo aca no cambia el tema: deja un valor "
                       "colgado que sobrevive a borrar saves. Se ignoro.\n"
                       "  Para cambiar el tema hay un comando: 'cargar_tema'.";
            }
            return "[VNScriptState] " + comando + ": '" + nombre + "' es estado interno del "
                   "motor y ademas viaja adentro de la partida guardada. "
                   "Escribirla desde el guion la corrompe. Se ignoro.\n"
                   "  Para cambiar el tema hay un comando: 'cargar_tema'.";
        }
        return std::string();
    }
}
