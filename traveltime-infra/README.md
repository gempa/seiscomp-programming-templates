# Travel-time table IDC infra

## About

A plugin to compute travel times for infrasound phases, particularily "Is".
Its configuration is actually a list of pairs of distance and celerity.
If the travel time has to be computed for a particular distance, it looks up the
first pair for which the distance is not greater than the input distance and uses
its celerity to compute the time. If the input distance is less than the distance
of the first element of the table, then its celerity is used.

## Configuration

First compile your plugin. The filename could be `tttidcinfra.so`. In this particular
example it is `tmpltttidcinfra.so`.

Then load the plugin into your application or global via `global.cfg`:

```
plugins = ${plugins}, tttidcinfra
```

Next define the available models for your interface. The interface name is the one
registered in your plugin with `REGISTER_TRAVELTIMETABLE`. In this example it is
`idcinfra`.

```
ttt.idcinfra.tables = IDC_2010
```

This will use the default travel time table which does not require any additional
configurations. To add more tables, the following configuration can be used:

```
ttt.idcinfra.tables = IDC_2010, CUSTOM
ttt.idcinfra.CUSTOM.distances = 0.0, 1.2, 20.0
ttt.idcinfra.CUSTOM.celerities = 0.33, 0.295, 0.303
```
