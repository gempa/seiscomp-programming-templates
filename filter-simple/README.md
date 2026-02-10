# Waveform filter

## About

A plugin to filter traces. This plugin can be used in any application which
supports filters.

## Configuration

First compile your plugin. The filename could be `fltxyz.so`. Then load
the plugin into any application via `global.cfg`:

```
plugins = ${plugins}, fltxyz
```

Now you can use your filter accoring to the registered name `XYZ`,
see `REGISTER_INPLACE_FILTER`.

```
filter = "XYZ(1,2,3)"
```

## scautopick usage

In scautopick the filter can be configured as shown above. In addition,
the trigger thresholds `trigOn` and `trigOff` must be chosen according
the the implementation. As you have implemented the filter, you should now
which values make a pick and which activate the picker again. If the
implemented filter is not meant as a trigger then you can still pipe it
into the `STALTA` and use the defaults.

## Testing

The simplest testing of your filter is to load some data into `scrttv`
and use the filter:

```
$ scrttv --filter "XYZ(1,2,3)" data.mseed
```
