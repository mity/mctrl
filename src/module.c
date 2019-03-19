/*
 * Copyright (c) 2009-2015 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "module.h"


/**************************
 *** Module abstraction ***
 **************************/

static mc_mutex_t mod_mutex;

void
module_dllmain_init(void)
{
    mc_mutex_init(&mod_mutex);
}

void
module_dllmain_fini(void)
{
    mc_mutex_fini(&mod_mutex);
}


/* Module structure */
typedef struct module_tag module_t;
struct module_tag {
#ifdef DEBUG
    const char* name;
#endif
    int (*fn_init)(void);    /* init function */
    void (*fn_fini)(void);   /* cleaning function */
    UINT refs;               /* number of initializations */
};


static int
module_init_modules(module_t** modules, int n)
{
    int res = 0;
    int i = 0;

    mc_mutex_lock(&mod_mutex);
    while(i < n) {
        if(modules[i]->refs == 0) {
            res = modules[i]->fn_init();
            if(MC_ERR(res != 0)) {
                MC_TRACE("module_init_modules: %s_init() failed.",
                         modules[i]->name);
                /* Rollback previous initializations */
                while(--i >= 0) {
                    modules[i]->refs--;
                    if(modules[i]->refs == 0)
                        modules[i]->fn_fini();
                }
                break;
            }
        }
        modules[i]->refs++;
        i++;
    }
    mc_mutex_unlock(&mod_mutex);
    return res;
}

static void
module_fini_modules(module_t** modules, int n)
{
    int i;

    mc_mutex_lock(&mod_mutex);
    for(i = n-1; i >= 0; i--) {
        MC_ASSERT(modules[i]->refs > 0);
        modules[i]->refs--;
        if(modules[i]->refs == 0)
            modules[i]->fn_fini();
    }
    mc_mutex_unlock(&mod_mutex);
}



/**************************************
 *** Macros for module declarations ***
 **************************************/

#ifdef DEBUG
    #define DEFINE_MODULE_NAME(mod_name)    #mod_name,
#else
    #define DEFINE_MODULE_NAME(mod_name)
#endif

/* Declaration of a module */
#define DEFINE_MODULE(mod_name)                                               \
    int mod_name##_init_module(void);                                         \
    void mod_name##_fini_module(void);                                        \
    static module_t mod_##mod_name = {                                        \
                DEFINE_MODULE_NAME(mod_name)                                  \
                mod_name##_init_module,                                       \
                mod_name##_fini_module,                                       \
                0                                                             \
    }

/* Macro implementing public interface for public modules */
#define DEFINE_PUBLIC_IFACE(mod_name, PublicName, deps)                       \
    BOOL MCTRL_API                                                            \
    mc##PublicName##_Initialize(void)                                         \
    {                                                                         \
        if(MC_ERR(module_init_modules(deps, MC_SIZEOF_ARRAY(deps)) != 0)) {   \
            MC_TRACE("mc%s_Initialize: module_init_modules() failed.",        \
                     MC_STRINGIZE(PublicName));                               \
            return FALSE;                                                     \
        }                                                                     \
        return TRUE;                                                          \
    }                                                                         \
                                                                              \
    void MCTRL_API                                                            \
    mc##PublicName##_Terminate(void)                                          \
    {                                                                         \
        module_fini_modules(deps, MC_SIZEOF_ARRAY(deps));                     \
    }


/*******************************
 *** The module declarations ***
 *******************************/

/* Module definition */

DEFINE_MODULE(mc);
DEFINE_MODULE(button);
DEFINE_MODULE(chart);
DEFINE_MODULE(dwm);
DEFINE_MODULE(expand);
DEFINE_MODULE(grid);
DEFINE_MODULE(html);
DEFINE_MODULE(imgview);
DEFINE_MODULE(mditab);
DEFINE_MODULE(menubar);
DEFINE_MODULE(theme);
DEFINE_MODULE(treelist);


/* Public interfaces of exposed modules */

static module_t* mod_button_deps[] = { &mod_mc, &mod_theme, &mod_button };
DEFINE_PUBLIC_IFACE(button, Button, mod_button_deps)

static module_t* mod_chart_deps[] = { &mod_mc, &mod_theme, &mod_chart };
DEFINE_PUBLIC_IFACE(chart, Chart, mod_chart_deps)

static module_t* mod_expand_deps[] = { &mod_mc, &mod_theme, &mod_expand };
DEFINE_PUBLIC_IFACE(expand, Expand, mod_expand_deps)

static module_t* mod_grid_deps[] = { &mod_mc, &mod_theme, &mod_grid };
DEFINE_PUBLIC_IFACE(grid, Grid, mod_grid_deps)

static module_t* mod_html_deps[] = { &mod_mc, &mod_theme, &mod_html };
DEFINE_PUBLIC_IFACE(html, Html, mod_html_deps)

static module_t* mod_imgview_deps[] = { &mod_mc, &mod_imgview };
DEFINE_PUBLIC_IFACE(imgview, ImgView, mod_imgview_deps)

static module_t* mod_mditab_deps[] = { &mod_mc, &mod_dwm, &mod_mditab };
DEFINE_PUBLIC_IFACE(mditab, Mditab, mod_mditab_deps)

static module_t* mod_menubar_deps[] = { &mod_mc, &mod_theme, &mod_menubar };
DEFINE_PUBLIC_IFACE(menubar, Menubar, mod_menubar_deps)

static module_t* mod_theme_deps[] = { &mod_mc, &mod_theme };
DEFINE_PUBLIC_IFACE(theme, Theme, mod_theme_deps)

static module_t* mod_treelist_deps[] = { &mod_mc, &mod_theme, &mod_treelist };
DEFINE_PUBLIC_IFACE(treelist, TreeList, mod_treelist_deps)
