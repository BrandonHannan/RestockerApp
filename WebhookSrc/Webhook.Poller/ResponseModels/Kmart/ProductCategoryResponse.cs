using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Poller.ResponseModels.Kmart
{
    public class ProductCategoryResponse
    {
        public class Rootobject
        {
            public Response response { get; set; }
            public string result_id { get; set; }
            public Request request { get; set; }
        }

        public class Response
        {
            public Result_Sources result_sources { get; set; }
            public Facet[] facets { get; set; }
            public Group[] groups { get; set; }
            public Result[] results { get; set; }
            public Sort_Options[] sort_options { get; set; }
            public object[] refined_content { get; set; }
            public int total_num_results { get; set; }
            public Feature[] features { get; set; }
            public Related_Searches[] related_searches { get; set; }
            public object[] related_browse_pages { get; set; }
        }

        public class Result_Sources
        {
            public Token_Match token_match { get; set; }
            public Embeddings_Match embeddings_match { get; set; }
        }

        public class Token_Match
        {
            public int count { get; set; }
        }

        public class Embeddings_Match
        {
            public int count { get; set; }
        }

        public class Facet
        {
            public string display_name { get; set; }
            public string name { get; set; }
            public string type { get; set; }
            public Option[] options { get; set; }
            public bool hidden { get; set; }
            public Data data { get; set; }
        }

        public class Data
        {
        }

        public class Option
        {
            public string status { get; set; }
            public int count { get; set; }
            public string display_name { get; set; }
            public string value { get; set; }
            public Data1 data { get; set; }
            public object[] range { get; set; }
        }

        public class Data1
        {
        }

        public class Group
        {
            public string group_id { get; set; }
            public string display_name { get; set; }
            public int count { get; set; }
            public Data2 data { get; set; }
            public object[] children { get; set; }
            public Parent[] parents { get; set; }
        }

        public class Data2
        {
            public string url { get; set; }
            public int sequence { get; set; }
            public bool isSpecial { get; set; }
            public string identifier { get; set; }
            public string[] breadcrumbs { get; set; }
            public bool defaultCategory { get; set; }
        }

        public class Parent
        {
            public string display_name { get; set; }
            public string group_id { get; set; }
        }

        public class Result
        {
            public object[] matched_terms { get; set; }
            public Labels labels { get; set; }
            public Data3 data { get; set; }
            public string value { get; set; }
            public bool is_slotted { get; set; }
            public Variation[] variations { get; set; }
        }

        public class Labels
        {
            public __Cnstrc_Is_Context_Bestseller __cnstrc_is_context_bestseller { get; set; }
            public __Cnstrc_Is_Global_Bestseller __cnstrc_is_global_bestseller { get; set; }
            public __Cnstrc_Is_New_Arrivals __cnstrc_is_new_arrivals { get; set; }
            public __Cnstrc_Is_Global_Trending_Now __cnstrc_is_global_trending_now { get; set; }
            public __Cnstrc_Is_Context_Trending_Now __cnstrc_is_context_trending_now { get; set; }
        }

        public class __Cnstrc_Is_Context_Bestseller
        {
            public string display_name { get; set; }
            public object value { get; set; }
        }

        public class __Cnstrc_Is_Global_Bestseller
        {
            public string display_name { get; set; }
            public object value { get; set; }
        }

        public class __Cnstrc_Is_New_Arrivals
        {
            public string display_name { get; set; }
            public object value { get; set; }
        }

        public class __Cnstrc_Is_Global_Trending_Now
        {
            public string display_name { get; set; }
            public object value { get; set; }
        }

        public class __Cnstrc_Is_Context_Trending_Now
        {
            public string display_name { get; set; }
            public object value { get; set; }
        }

        public class Data3
        {
            public string id { get; set; }
            public string uri { get; set; }
            public string url { get; set; }
            public Video video { get; set; }
            public string Brand { get; set; }
            public string[] badges { get; set; }
            public bool arEnabled { get; set; }
            public string image_url { get; set; }
            public string[] altImages { get; set; }
            public bool FreeShipping { get; set; }
            public string MerchClassName { get; set; }
            public int MerchDepartment { get; set; }
            public bool isPreOrderActive { get; set; }
            public bool AssortedProducts { get; set; }
            public int FulfilmentChannel { get; set; }
            public bool FreeShippingMetro { get; set; }
            public string primaryCategoryId { get; set; }
            public bool nationalInventory { get; set; }
            public string preOrderReleaseDate { get; set; }
            public Ratings ratings { get; set; }
            public bool flatRateBigBulkyMetro { get; set; }
            public int saleEffectiveDateTime { get; set; }
            public Badgesmarketplace badgesMarketplace { get; set; }
            public string[] group_ids { get; set; }
            public long apn { get; set; }
            public string Size { get; set; }
            public float price { get; set; }
            public string[] Seller { get; set; }
            public Price[] prices { get; set; }
            public string Colour { get; set; }
            public bool clearance { get; set; }
            public bool is_default { get; set; }
            public string variation_id { get; set; }
            public Variant_Video variant_video { get; set; }
            public object[] variant_badges { get; set; }
            public string SecondaryColour { get; set; }
            public Stateoos stateOOS { get; set; }
            public Offer[] offers { get; set; }
        }

        public class Video
        {
        }

        public class Ratings
        {
            public float averageScore { get; set; }
            public int totalReviews { get; set; }
        }

        public class Badgesmarketplace
        {
            public Seller[] seller { get; set; }
        }

        public class Seller
        {
            public string id { get; set; }
            public string type { get; set; }
            public int order { get; set; }
            public string variant { get; set; }
            public string displayText { get; set; }
            public string displayLabel { get; set; }
            public int count { get; set; }
        }

        public class Variant_Video
        {
            public string youtubeID { get; set; }
        }

        public class Stateoos
        {
            public string ACT { get; set; }
            public string SA { get; set; }
            public string WA { get; set; }
            public string VIC { get; set; }
            public string NSW { get; set; }
            public string QLD { get; set; }
            public string NT { get; set; }
            public string TAS { get; set; }
        }

        public class Price
        {
            public string type { get; set; }
            public string amount { get; set; }
            public string country { get; set; }
            public string endDate { get; set; }
            public string currency { get; set; }
            public string startDate { get; set; }
        }

        public class Offer
        {
            public int id { get; set; }
            public int order { get; set; }
            public Badges badges { get; set; }
            public Price1[] prices { get; set; }
            public int shopId { get; set; }
            public int quantity { get; set; }
            public string shopName { get; set; }
            public bool cheapestOffer { get; set; }
            public bool fastestDelivery { get; set; }
            public string fulfilmentChannel { get; set; }
            public Offeradditionalfield[] offerAdditionalFields { get; set; }
        }

        public class Badges
        {
            public Seller1[] seller { get; set; }
        }

        public class Seller1
        {
            public string id { get; set; }
            public string type { get; set; }
            public int order { get; set; }
            public string displayText { get; set; }
        }

        public class Price1
        {
            public float originPrice { get; set; }
        }

        public class Offeradditionalfield
        {
            public string code { get; set; }
            public string type { get; set; }
            public string value { get; set; }
        }

        public class Variation
        {
            public Data4 data { get; set; }
            public string value { get; set; }
        }

        public class Data4
        {
            public long apn { get; set; }
            public string Size { get; set; }
            public float price { get; set; }
            public string[] Seller { get; set; }
            public Price2[] prices { get; set; }
            public string Colour { get; set; }
            public string image_url { get; set; }
            public string[] altImages { get; set; }
            public bool clearance { get; set; }
            public bool is_default { get; set; }
            public string variation_id { get; set; }
            public Variant_Video1 variant_video { get; set; }
            public object[] variant_badges { get; set; }
            public string SecondaryColour { get; set; }
            public int saleEffectiveDateTime { get; set; }
            public Stateoos1 stateOOS { get; set; }
            public Offer1[] offers { get; set; }
        }

        public class Variant_Video1
        {
            public string youtubeID { get; set; }
        }

        public class Stateoos1
        {
            public string ACT { get; set; }
            public string SA { get; set; }
            public string WA { get; set; }
            public string VIC { get; set; }
            public string NSW { get; set; }
            public string QLD { get; set; }
            public string NT { get; set; }
            public string TAS { get; set; }
        }

        public class Price2
        {
            public string type { get; set; }
            public string amount { get; set; }
            public string country { get; set; }
            public string endDate { get; set; }
            public string currency { get; set; }
            public string startDate { get; set; }
        }

        public class Offer1
        {
            public int id { get; set; }
            public int order { get; set; }
            public Badges1 badges { get; set; }
            public Price3[] prices { get; set; }
            public int shopId { get; set; }
            public int quantity { get; set; }
            public string shopName { get; set; }
            public bool cheapestOffer { get; set; }
            public bool fastestDelivery { get; set; }
            public string fulfilmentChannel { get; set; }
            public Offeradditionalfield1[] offerAdditionalFields { get; set; }
        }

        public class Badges1
        {
            public Seller2[] seller { get; set; }
        }

        public class Seller2
        {
            public string id { get; set; }
            public string type { get; set; }
            public int order { get; set; }
            public string displayText { get; set; }
        }

        public class Price3
        {
            public float originPrice { get; set; }
        }

        public class Offeradditionalfield1
        {
            public string code { get; set; }
            public string type { get; set; }
            public string value { get; set; }
        }

        public class Sort_Options
        {
            public string sort_by { get; set; }
            public string display_name { get; set; }
            public string sort_order { get; set; }
            public string status { get; set; }
            public bool hidden { get; set; }
        }

        public class Feature
        {
            public string feature_name { get; set; }
            public string display_name { get; set; }
            public bool enabled { get; set; }
            public Variant variant { get; set; }
        }

        public class Variant
        {
            public string name { get; set; }
            public string display_name { get; set; }
        }

        public class Related_Searches
        {
            public string query { get; set; }
        }

        public class Request
        {
            public string sort_by { get; set; }
            public string sort_order { get; set; }
            public int page { get; set; }
            public int num_results_per_page { get; set; }
            public string term { get; set; }
            public Fmt_Options fmt_options { get; set; }
            public string section { get; set; }
            public Features features { get; set; }
            public Feature_Variants feature_variants { get; set; }
            public Searchandized_Items searchandized_items { get; set; }
            public string browse_filter_name { get; set; }
            public string browse_filter_value { get; set; }
        }

        public class Fmt_Options
        {
            public string groups_start { get; set; }
            public int groups_max_depth { get; set; }
            public bool show_hidden_facets { get; set; }
            public bool show_hidden_fields { get; set; }
            public bool show_protected_facets { get; set; }
        }

        public class Features
        {
            public bool query_items { get; set; }
            public bool a_a_test { get; set; }
            public bool auto_generated_refined_query_rules { get; set; }
            public bool manual_searchandizing { get; set; }
            public bool personalization { get; set; }
            public bool filter_items { get; set; }
            public bool use_reranker_service_for_search { get; set; }
            public bool use_reranker_service_for_browse { get; set; }
            public bool use_reranker_service_for_all { get; set; }
            public bool custom_autosuggest_ui { get; set; }
            public bool disable_test_only_global_rules_search { get; set; }
            public bool disable_test_only_global_rules_browse { get; set; }
            public bool use_enriched_attributes_as_fuzzy_searchable { get; set; }
            public bool recommendations_merge_allowlist_rules { get; set; }
            public bool reranker_transformations_browse { get; set; }
            public bool variation_selection { get; set; }
            public bool refined_tag_rules { get; set; }
            public bool reranker_transformations_search { get; set; }
            public bool reranker_transformations_autocomplete { get; set; }
            public bool use_llm_label_sort { get; set; }
            public bool disable_test_only_tag_rules { get; set; }
            public bool use_ces_blending { get; set; }
        }

        public class Feature_Variants
        {
            public string query_items { get; set; }
            public object a_a_test { get; set; }
            public string auto_generated_refined_query_rules { get; set; }
            public object manual_searchandizing { get; set; }
            public string personalization { get; set; }
            public string filter_items { get; set; }
            public string use_reranker_service_for_search { get; set; }
            public string use_reranker_service_for_browse { get; set; }
            public object use_reranker_service_for_all { get; set; }
            public string custom_autosuggest_ui { get; set; }
            public object disable_test_only_global_rules_search { get; set; }
            public object disable_test_only_global_rules_browse { get; set; }
            public object use_enriched_attributes_as_fuzzy_searchable { get; set; }
            public object recommendations_merge_allowlist_rules { get; set; }
            public object reranker_transformations_browse { get; set; }
            public object variation_selection { get; set; }
            public object refined_tag_rules { get; set; }
            public object reranker_transformations_search { get; set; }
            public object reranker_transformations_autocomplete { get; set; }
            public object use_llm_label_sort { get; set; }
            public object disable_test_only_tag_rules { get; set; }
            public object use_ces_blending { get; set; }
        }

        public class Searchandized_Items
        {
            public object[] P_43279746 { get; set; }
            public object[] P_43665945 { get; set; }
            public object[] P_43485390 { get; set; }
            public object[] P_43767236 { get; set; }
            public object[] P_43737048 { get; set; }
        }
    }
}
