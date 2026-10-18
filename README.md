# casioPrizmApps

A collection of fx-CG50 (CG10/CG20) add-ins built with **fxSDK** and **gint**.
Every app lives in `apps/<name>/` and compiles to its own `.g3a`; all of them are
collected in `dist/`.

```
casioPrizmApps/
├── apps/               one folder per add-in
│   └── base-n/         BaseN converter
│       ├── src/        app source code
│       ├── helpers/    app-specific helper code (optional)
│       ├── assets-cg/  icons + images for fx-CG
│       ├── assets-fx/  icons + images for fx-9860G
│       └── CMakeLists.txt
├── common/             helpers shared by every app (utils.h/.c, common.cmake)
├── template/           skeleton used by `make new`
├── scripts/            build.sh, new-app.sh
└── dist/               compiled .g3a files for every app
```

## Usage

```sh
make                          # build all apps -> dist/*.g3a
make app APP=base-n           # build one app
make send APP=base-n          # build and send to a connected calculator
make new APP=my-app NAME=MyApp  # create apps/my-app from the template
make clean
```

In VS Code: `Ctrl+Shift+B` builds everything; other tasks are under *Run Task*.

## Adding code to an app

List new `.c` files under `SOURCES` in the app's `CMakeLists.txt`, and images
under `ASSETS_cg` (with an entry in `assets-cg/fxconv-metadata.txt`).
Shared code goes in `common/src` + `common/include` and is linked into every app.
