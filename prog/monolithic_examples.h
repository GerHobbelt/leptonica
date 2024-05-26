
#pragma once

#if defined(BUILD_MONOLITHIC)

#ifdef __cplusplus
extern "C" {
#endif

int lept_adaptmap_dark_main(int argc, const char **argv);
int lept_adaptmap_reg_main(int argc, const char **argv);
int lept_adaptnorm_reg_main(int argc, const char **argv);
int lept_affine_reg_main(int argc, const char **argv);
int lept_alltests_reg_main(int argc, const char **argv);
int lept_alphaops_reg_main(int argc, const char **argv);
int lept_alphaxform_reg_main(int argc, const char **argv);
int lept_arabic_lines_main(int argc, const char **argv);
int lept_arithtest_main(int argc, const char **argv);
int lept_autogentest1_main(int argc, const char **argv);
int lept_autogentest2_main(int argc, const char **argv);
int lept_barcodetest_main(int argc, const char **argv);
int lept_baseline_reg_main(int argc, const char **argv);
int lept_bilateral1_reg_main(int argc, const char **argv);
int lept_bilateral2_reg_main(int argc, const char **argv);
int lept_bilinear_reg_main(int argc, const char **argv);
int lept_binarize_reg_main(int argc, const char **argv);
int lept_binarize_set_main(int argc, const char **argv);
int lept_binarizefiles_main(int argc, const char **argv);
int lept_bincompare_main(int argc, const char **argv);
int lept_binmorph1_reg_main(int argc, const char **argv);
int lept_binmorph2_reg_main(int argc, const char **argv);
int lept_binmorph3_reg_main(int argc, const char **argv);
int lept_binmorph4_reg_main(int argc, const char **argv);
int lept_binmorph5_reg_main(int argc, const char **argv);
int lept_binmorph6_reg_main(int argc, const char **argv);
int lept_blackwhite_reg_main(int argc, const char **argv);
int lept_blend1_reg_main(int argc, const char **argv);
int lept_blend2_reg_main(int argc, const char **argv);
int lept_blend3_reg_main(int argc, const char **argv);
int lept_blend4_reg_main(int argc, const char **argv);
int lept_blend5_reg_main(int argc, const char **argv);
int lept_blendcmaptest_main(int argc, const char **argv);
int lept_boxa1_reg_main(int argc, const char **argv);
int lept_boxa2_reg_main(int argc, const char **argv);
int lept_boxa3_reg_main(int argc, const char **argv);
int lept_boxa4_reg_main(int argc, const char **argv);
int lept_buffertest_main(int argc, const char **argv);
int lept_bytea_reg_main(int argc, const char **argv);
int lept_ccbord_reg_main(int argc, const char **argv);
int lept_ccbordtest_main(int argc, const char **argv);
int lept_cctest1_main(int argc, const char **argv);
int lept_ccthin1_reg_main(int argc, const char **argv);
int lept_ccthin2_reg_main(int argc, const char **argv);
int lept_checkerboard_reg_main(int argc, const char **argv);
int lept_circle_reg_main(int argc, const char **argv);
int lept_cleanpdf_main(int argc, const char **argv);
int lept_cmapquant_reg_main(int argc, const char **argv);
int lept_colorcontent_reg_main(int argc, const char **argv);
int lept_colorfill_reg_main(int argc, const char **argv);
int lept_coloring_reg_main(int argc, const char **argv);
int lept_colorize_reg_main(int argc, const char **argv);
int lept_colormask_reg_main(int argc, const char **argv);
int lept_colormorph_reg_main(int argc, const char **argv);
int lept_colorquant_reg_main(int argc, const char **argv);
int lept_colorseg_reg_main(int argc, const char **argv);
int lept_colorsegtest_main(int argc, const char **argv);
int lept_colorspace_reg_main(int argc, const char **argv);
int lept_compare_reg_main(int argc, const char **argv);
int lept_comparepages_main(int argc, const char **argv);
int lept_comparepixa_main(int argc, const char **argv);
int lept_comparetest_main(int argc, const char **argv);
int lept_compfilter_reg_main(int argc, const char **argv);
int lept_compresspdf_main(int argc, const char **argv);
int lept_conncomp_reg_main(int argc, const char **argv);
int lept_contrasttest_main(int argc, const char **argv);
int lept_conversion_reg_main(int argc, const char **argv);
int lept_convertfilestopdf_main(int argc, const char **argv);
int lept_convertfilestops_main(int argc, const char **argv);
int lept_convertformat_main(int argc, const char **argv);
int lept_convertsegfilestopdf_main(int argc, const char **argv);
int lept_convertsegfilestops_main(int argc, const char **argv);
int lept_converttogray_main(int argc, const char **argv);
int lept_converttopdf_main(int argc, const char **argv);
int lept_converttops_main(int argc, const char **argv);
int lept_convolve_reg_main(int argc, const char **argv);
int lept_cornertest_main(int argc, const char **argv);
int lept_corrupttest_main(int argc, const char **argv);
int lept_crop_reg_main(int argc, const char **argv);
int lept_croppdf_main(int argc, const char **argv);
int lept_croptext_main(int argc, const char **argv);
int lept_custom_log_plot_test_main(int argc, const char **argv);
int lept_deskew_it_main(int argc, const char **argv);
int lept_dewarp_reg_main(int argc, const char **argv);
int lept_dewarprules_main(int argc, const char **argv);
int lept_dewarptest1_main(int argc, const char **argv);
int lept_dewarptest2_main(int argc, const char **argv);
int lept_dewarptest3_main(int argc, const char **argv);
int lept_dewarptest4_main(int argc, const char **argv);
int lept_dewarptest5_main(int argc, const char **argv);
int lept_digitprep1_main(int argc, const char **argv);
int lept_displayboxa_main(int argc, const char **argv);
int lept_displayboxes_on_pixa_main(int argc, const char **argv);
int lept_displaypix_main(int argc, const char **argv);
int lept_displaypixa_main(int argc, const char **argv);
int lept_distance_reg_main(int argc, const char **argv);
int lept_dither_reg_main(int argc, const char **argv);
int lept_dna_reg_main(int argc, const char **argv);
int lept_dwalineargen_main(int argc, const char **argv);
int lept_dwamorph1_reg_main(int argc, const char **argv);
int lept_dwamorph2_reg_main(int argc, const char **argv);
int lept_edge_reg_main(int argc, const char **argv);
int lept_encoding_reg_main(int argc, const char **argv);
int lept_enhance_reg_main(int argc, const char **argv);
int lept_equal_reg_main(int argc, const char **argv);
int lept_expand_reg_main(int argc, const char **argv);
int lept_extrema_reg_main(int argc, const char **argv);
int lept_falsecolor_reg_main(int argc, const char **argv);
int lept_fcombautogen_main(int argc, const char **argv);
int lept_fhmtauto_reg_main(int argc, const char **argv);
int lept_fhmtautogen_main(int argc, const char **argv);
int lept_fileinfo_main(int argc, const char **argv);
int lept_files_reg_main(int argc, const char **argv);
int lept_find_colorregions_main(int argc, const char **argv);
int lept_findbinding_main(int argc, const char **argv);
int lept_findcorners_reg_main(int argc, const char **argv);
int lept_findpattern1_main(int argc, const char **argv);
int lept_findpattern2_main(int argc, const char **argv);
int lept_findpattern3_main(int argc, const char **argv);
int lept_findpattern1_reg_main(int argc, const char **argv);
int lept_findpattern2_reg_main(int argc, const char **argv);
int lept_flipdetect_reg_main(int argc, const char **argv);
int lept_fmorphauto_reg_main(int argc, const char **argv);
int lept_fmorphautogen_main(int argc, const char **argv);
int lept_fpix1_reg_main(int argc, const char **argv);
int lept_fpix2_reg_main(int argc, const char **argv);
int lept_fpixcontours_main(int argc, const char **argv);
int lept_gammatest_main(int argc, const char **argv);
int lept_genfonts_reg_main(int argc, const char **argv);
int lept_gifio_reg_main(int argc, const char **argv);
int lept_graphicstest_main(int argc, const char **argv);
int lept_grayfill_reg_main(int argc, const char **argv);
int lept_graymorph1_reg_main(int argc, const char **argv);
int lept_graymorph2_reg_main(int argc, const char **argv);
int lept_graymorphtest_main(int argc, const char **argv);
int lept_grayquant_reg_main(int argc, const char **argv);
int lept_hardlight_reg_main(int argc, const char **argv);
int lept_hash_reg_main(int argc, const char **argv);
int lept_hashtest_main(int argc, const char **argv);
int lept_heap_reg_main(int argc, const char **argv);
int lept_histoduptest_main(int argc, const char **argv);
int lept_histotest_main(int argc, const char **argv);
int lept_htmlviewer_main(int argc, const char **argv);
int lept_imagetops_main(int argc, const char **argv);
int lept_insert_reg_main(int argc, const char **argv);
int lept_ioformats_reg_main(int argc, const char **argv);
int lept_iomisc_reg_main(int argc, const char **argv);
int lept_italic_reg_main(int argc, const char **argv);
int lept_jbclass_reg_main(int argc, const char **argv);
int lept_jbcorrelation_main(int argc, const char **argv);
int lept_jbrankhaus_main(int argc, const char **argv);
int lept_jbwords_main(int argc, const char **argv);
int lept_jp2kio_reg_main(int argc, const char **argv);
int lept_jpegio_reg_main(int argc, const char **argv);
int lept_kernel_reg_main(int argc, const char **argv);
int lept_label_reg_main(int argc, const char **argv);
int lept_lightcolortest_main(int argc, const char **argv);
int lept_lineremoval_reg_main(int argc, const char **argv);
int lept_listtest_main(int argc, const char **argv);
int lept_livre_adapt_main(int argc, const char **argv);
int lept_livre_hmt_main(int argc, const char **argv);
int lept_livre_makefigs_main(int argc, const char **argv);
int lept_livre_orient_main(int argc, const char **argv);
int lept_livre_pageseg_main(int argc, const char **argv);
int lept_livre_seedgen_main(int argc, const char **argv);
int lept_livre_tophat_main(int argc, const char **argv);
int lept_locminmax_reg_main(int argc, const char **argv);
int lept_logicops_reg_main(int argc, const char **argv);
int lept_lowaccess_reg_main(int argc, const char **argv);
int lept_lowsat_reg_main(int argc, const char **argv);
int lept_maketile_main(int argc, const char **argv);
int lept_maptest_main(int argc, const char **argv);
int lept_maze_reg_main(int argc, const char **argv);
int lept_misctest1_main(int argc, const char **argv);
int lept_modifyhuesat_main(int argc, const char **argv);
int lept_morphseq_reg_main(int argc, const char **argv);
int lept_morphtest1_main(int argc, const char **argv);
int lept_mtiff_reg_main(int argc, const char **argv);
int lept_multitype_reg_main(int argc, const char **argv);
int lept_nearline_reg_main(int argc, const char **argv);
int lept_newspaper_reg_main(int argc, const char **argv);
int lept_numa1_reg_main(int argc, const char **argv);
int lept_numa2_reg_main(int argc, const char **argv);
int lept_numa3_reg_main(int argc, const char **argv);
int lept_numaranktest_main(int argc, const char **argv);
int lept_otsutest1_main(int argc, const char **argv);
int lept_otsutest2_main(int argc, const char **argv);
int lept_overlap_reg_main(int argc, const char **argv);
int lept_pageseg_reg_main(int argc, const char **argv);
int lept_pagesegtest1_main(int argc, const char **argv);
int lept_pagesegtest2_main(int argc, const char **argv);
int lept_paint_reg_main(int argc, const char **argv);
int lept_paintmask_reg_main(int argc, const char **argv);
int lept_partifytest_main(int argc, const char **argv);
int lept_partition_reg_main(int argc, const char **argv);
int lept_partitiontest_main(int argc, const char **argv);
int lept_pdfio1_reg_main(int argc, const char **argv);
int lept_pdfio2_reg_main(int argc, const char **argv);
int lept_pdfseg_reg_main(int argc, const char **argv);
int lept_percolatetest_main(int argc, const char **argv);
int lept_pixa1_reg_main(int argc, const char **argv);
int lept_pixa2_reg_main(int argc, const char **argv);
int lept_pixaatest_main(int argc, const char **argv);
int lept_pixadisp_reg_main(int argc, const char **argv);
int lept_pixafileinfo_main(int argc, const char **argv);
int lept_pixalloc_reg_main(int argc, const char **argv);
int lept_pixcomp_reg_main(int argc, const char **argv);
int lept_pixmem_reg_main(int argc, const char **argv);
int lept_pixserial_reg_main(int argc, const char **argv);
int lept_pixtile_reg_main(int argc, const char **argv);
int lept_plottest_main(int argc, const char **argv);
int lept_pngio_reg_main(int argc, const char **argv);
int lept_pnmio_reg_main(int argc, const char **argv);
int lept_printimage_main(int argc, const char **argv);
int lept_printsplitimage_main(int argc, const char **argv);
int lept_printtiff_main(int argc, const char **argv);
int lept_projection_reg_main(int argc, const char **argv);
int lept_projective_reg_main(int argc, const char **argv);
int lept_psio_reg_main(int argc, const char **argv);
int lept_psioseg_reg_main(int argc, const char **argv);
int lept_pta_reg_main(int argc, const char **argv);
int lept_ptra1_reg_main(int argc, const char **argv);
int lept_ptra2_reg_main(int argc, const char **argv);
int lept_quadtree_reg_main(int argc, const char **argv);
int lept_rank_reg_main(int argc, const char **argv);
int lept_rankbin_reg_main(int argc, const char **argv);
int lept_rankhisto_reg_main(int argc, const char **argv);
int lept_rasterop_reg_main(int argc, const char **argv);
int lept_rasteropip_reg_main(int argc, const char **argv);
int lept_rasteroptest_main(int argc, const char **argv);
int lept_rbtreetest_main(int argc, const char **argv);
int lept_recog_bootnum1_main(int argc, const char **argv);
int lept_recog_bootnum2_main(int argc, const char **argv);
int lept_recog_bootnum3_main(int argc, const char **argv);
int lept_recogsort_main(int argc, const char **argv);
int lept_recogtest1_main(int argc, const char **argv);
int lept_recogtest2_main(int argc, const char **argv);
int lept_recogtest3_main(int argc, const char **argv);
int lept_recogtest4_main(int argc, const char **argv);
int lept_recogtest5_main(int argc, const char **argv);
int lept_recogtest6_main(int argc, const char **argv);
int lept_recogtest7_main(int argc, const char **argv);
int lept_rectangle_reg_main(int argc, const char **argv);
int lept_reducetest_main(int argc, const char **argv);
int lept_removecmap_main(int argc, const char **argv);
int lept_renderfonts_main(int argc, const char **argv);
int lept_replacebytes_main(int argc, const char **argv);
int lept_rotate1_reg_main(int argc, const char **argv);
int lept_rotate2_reg_main(int argc, const char **argv);
int lept_rotate_it_main(int argc, const char **argv);
int lept_rotatefastalt_main(int argc, const char **argv);
int lept_rotateorth_reg_main(int argc, const char **argv);
int lept_rotateorthtest1_main(int argc, const char **argv);
int lept_rotatetest1_main(int argc, const char **argv);
int lept_runlengthtest_main(int argc, const char **argv);
int lept_scale_it_main(int argc, const char **argv);
int lept_scale_reg_main(int argc, const char **argv);
int lept_scaleandtile_main(int argc, const char **argv);
int lept_scaletest1_main(int argc, const char **argv);
int lept_scaletest2_main(int argc, const char **argv);
int lept_scaleimages_main(int argc, const char **argv);
int lept_seedfilltest_main(int argc, const char **argv);
int lept_seedspread_reg_main(int argc, const char **argv);
int lept_selio_reg_main(int argc, const char **argv);
int lept_settest_main(int argc, const char **argv);
int lept_sharptest_main(int argc, const char **argv);
int lept_shear1_reg_main(int argc, const char **argv);
int lept_shear2_reg_main(int argc, const char **argv);
int lept_sheartest_main(int argc, const char **argv);
int lept_showedges_main(int argc, const char **argv);
int lept_skew_reg_main(int argc, const char **argv);
int lept_skewtest_main(int argc, const char **argv);
int lept_smallpix_reg_main(int argc, const char **argv);
int lept_smoothedge_reg_main(int argc, const char **argv);
int lept_sorttest_main(int argc, const char **argv);
int lept_speckle_reg_main(int argc, const char **argv);
int lept_splitcomp_reg_main(int argc, const char **argv);
int lept_splitimage2pdf_main(int argc, const char **argv);
int lept_splitpdf_main(int argc, const char **argv);
int lept_string_reg_main(int argc, const char **argv);
int lept_subpixel_reg_main(int argc, const char **argv);
int lept_sudokutest_main(int argc, const char **argv);
int lept_textorient_main(int argc, const char **argv);
int lept_texturefill_reg_main(int argc, const char **argv);
int lept_threshnorm_reg_main(int argc, const char **argv);
int lept_thresholding_test_main(int argc, const char **argv);
int lept_tiffpdftest_main(int argc, const char **argv);
int lept_translate_reg_main(int argc, const char **argv);
int lept_trctest_main(int argc, const char **argv);
int lept_underlinetest_main(int argc, const char **argv);
int lept_warper_reg_main(int argc, const char **argv);
int lept_warpertest_main(int argc, const char **argv);
int lept_watershed_reg_main(int argc, const char **argv);
int lept_webpanimio_reg_main(int argc, const char **argv);
int lept_webpio_reg_main(int argc, const char **argv);
int lept_wordboxes_reg_main(int argc, const char **argv);
int lept_wordsinorder_main(int argc, const char **argv);
int lept_writemtiff_main(int argc, const char **argv);
int lept_writetext_reg_main(int argc, const char **argv);
int lept_xformbox_reg_main(int argc, const char **argv);
int lept_yuvtest_main(int argc, const char **argv);
int lept_issue675_check_main(int argc, const char **argv);
int lept_message_test_main(void);
int lept_misctest2_main(int argc, const char **argv);

int lept_demo_pix_apis_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif
