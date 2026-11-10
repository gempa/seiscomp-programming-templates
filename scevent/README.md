# scevent score

## About

A plugin to update an event object based on HTTP REST API, e.g. to set
region names, time zones and so on.

## Configuration

The SCORE can be used along with other priorities, such as AGENCY or STATUS.
The only difference is that the SCORE evaluation function is not hardcoded
into scevent but must be provided by the user via plugins.

First compile your plugin. The filename could be `evupdrest.so`. Then load
the plugin into scevent via `scevent.cfg`:

```
plugins = ${plugins}, evupdrest
```
