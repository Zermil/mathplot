# math-plot

Crude mockery of what [matplotlib](https://matplotlib.org/) is, made using [gfx-template](https://github.com/Zermil/gfx-template) so that I can better test it and see what's missing and what's annoying.

## Code explanation

`base` - Helpers and useful 'standard library' functions.  
`gfx` - Deals with opening a window on a specific OS.  
`os` - Deals with OS specific stuff; win32, linux etc.  
`render` - Deals with backend API specific stuff; d3d11, opengl etc.  
`font` - Different font providers, font rasterization and rendering.  

If someone is taken a little aback by the use of linked list, there's [this great article](https://www.rfleury.com/p/in-defense-of-linked-lists) by [Ryan Fleury](https://twitter.com/ryanjfleury). Short version is that they work very well with arenas and as free-list for quick allocations.

## build

Make sure to change `MSVC_PATH` in the build script.

```console
> build
```
