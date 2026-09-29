# Building generator third-party software

The local `buildings-6.0.0.js` bundle contains the exact dependency versions
recorded in `package-lock.json`: polygon-clipping 0.15.7, earcut 3.2.3,
navcat 0.4.1, straight-skeleton 3.0.0, and @lume/kiwi 0.4.4.

Their license notices are retained by esbuild in the generated bundle. The
canonical license texts are also available in each package directory under
`node_modules` after `npm ci`.
