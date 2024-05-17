#include "internal.h"

#include <string>
#include <stdio.h>

MyInstance _instance;

static VkBool32 VKAPI_PTR vulkan_custom_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData) {
    printf("VKL: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

void init_extern(bool debug) {
    #ifndef VKDISPATCH_USE_VOLK
    setenv("MVK_CONFIG_LOG_LEVEL", "2", 0);
    #else
    LOG_INFO("Loading Vulkan using volk");
    VK_CALL(volkInitialize());
    #endif

    LOG_INFO("Initializing glslang...");

    if(!glslang_initialize_process()) {
        LOG_ERROR("Failed to initialize glslang process");
        return;
    }

    LOG_INFO("Initializing Vulkan Instance...");

    uint32_t instanceVersion;
    VK_CALL(vkEnumerateInstanceVersion(&instanceVersion));
    LOG_INFO("Instance API Version: %d.%d.%d", VK_API_VERSION_MAJOR(instanceVersion), VK_API_VERSION_MINOR(instanceVersion), VK_API_VERSION_PATCH(instanceVersion));

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vkdispatch";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "vkdispatch";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateFlags flags = 0;

    std::vector<const char *> extensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    std::vector<const char *> layers = {
        "VK_LAYER_KHRONOS_validation"
    };


#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    uint32_t layer_count = 0;
    VK_CALL(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
    std::vector<VkLayerProperties> instance_layers(layer_count);
    VK_CALL(vkEnumerateInstanceLayerProperties(&layer_count, instance_layers.data()));

    uint32_t extension_count = 0;
    VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr));
    std::vector<VkExtensionProperties> instance_extensions(extension_count);
    VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, instance_extensions.data()));

    // Check if layers are supported and remove the ones that aren't
    std::vector<const char*> supportedLayers;
    for (const char* layer : layers) {
        auto it = std::find_if(instance_layers.begin(), instance_layers.end(), [&](const VkLayerProperties& prop) {
            return strcmp(layer, prop.layerName) == 0;
        });
        if (it != instance_layers.end()) {
            LOG_INFO("Layer '%s' is supported", layer);
            supportedLayers.push_back(layer);
        } else {
            LOG_INFO("Layer '%s' is not supported", layer);
        }
    }

    // Check if extensions are supported and remove the ones that aren't
    std::vector<const char*> supportedExtensions;
    for (const char* extension : extensions) {
        auto it = std::find_if(instance_extensions.begin(), instance_extensions.end(), [&](const VkExtensionProperties& prop) {
            return strcmp(extension, prop.extensionName) == 0;
        });
        if (it != instance_extensions.end()) {
            LOG_INFO("Extension '%s' is supported", extension);
            supportedExtensions.push_back(extension);
        } else {
            LOG_INFO("Extension '%s' is not supported", extension);
        }
    }

	VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
    };
    
    VkValidationFeaturesEXT validationFeatures = {};
	validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
	validationFeatures.enabledValidationFeatureCount = sizeof(enabledValidationFeatures) / sizeof(enabledValidationFeatures[0]);
	validationFeatures.pEnabledValidationFeatures = enabledValidationFeatures;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = &validationFeatures;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.flags = flags;
    instanceCreateInfo.enabledExtensionCount = supportedExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = supportedExtensions.data();
    instanceCreateInfo.enabledLayerCount = supportedLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = supportedLayers.data();

    VK_CALL(vkCreateInstance(&instanceCreateInfo, nullptr, &_instance.instance));

    #ifdef VKDISPATCH_USE_VOLK
	volkLoadInstance(_instance.instance);
	#endif

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.pNext = NULL;
    debugCreateInfo.flags = 0;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugCreateInfo.pfnUserCallback = vulkan_custom_debug_callback;
    debugCreateInfo.pUserData = NULL;

    VK_CALL(vkCreateDebugUtilsMessengerEXT(_instance.instance, &debugCreateInfo, nullptr, &_instance.debug_messenger));

    LOG_INFO("Initializing Vulkan Devices...");

    uint32_t device_count;
    VK_CALL(vkEnumeratePhysicalDevices(_instance.instance, &device_count, nullptr));
    _instance.physicalDevices.resize(device_count);
    _instance.features.resize(device_count);
    _instance.atomicFloatFeatures.resize(device_count);
    _instance.properties.resize(device_count);
    _instance.subgroup_properties.resize(device_count);
    _instance.device_details.resize(device_count);
    VK_CALL(vkEnumeratePhysicalDevices(_instance.instance, &device_count, _instance.physicalDevices.data()));

    for(int i = 0; i < _instance.physicalDevices.size(); i++) {
        _instance.atomicFloatFeatures[i] = {};
        _instance.atomicFloatFeatures[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
        
        _instance.features[i] = {};
        _instance.features[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        _instance.features[i].pNext = &_instance.atomicFloatFeatures[i];
        
        vkGetPhysicalDeviceFeatures2(_instance.physicalDevices[i], &_instance.features[i]);

        VkPhysicalDeviceFeatures features = _instance.features[i].features;
        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures = _instance.atomicFloatFeatures[i];

        _instance.subgroup_properties[i] = {};
        _instance.subgroup_properties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

        _instance.properties[i] = {};
        _instance.properties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        _instance.properties[i].pNext = &_instance.subgroup_properties[i];

        vkGetPhysicalDeviceProperties2(_instance.physicalDevices[i], &_instance.properties[i]);

        VkPhysicalDeviceProperties properties = _instance.properties[i].properties;
        VkPhysicalDeviceSubgroupProperties subgroupProperties = _instance.subgroup_properties[i];

        _instance.device_details[i].version_variant = VK_API_VERSION_VARIANT(properties.apiVersion);
        _instance.device_details[i].version_major = VK_API_VERSION_MAJOR(properties.apiVersion);
        _instance.device_details[i].version_minor = VK_API_VERSION_MINOR(properties.apiVersion);
        _instance.device_details[i].version_patch = VK_API_VERSION_PATCH(properties.apiVersion);

        _instance.device_details[i].driver_version = properties.driverVersion;
        _instance.device_details[i].vendor_id = properties.vendorID;
        _instance.device_details[i].device_id = properties.deviceID;

        _instance.device_details[i].device_type = properties.deviceType;

        size_t deviceNameLength = strlen(properties.deviceName) + 1;
        _instance.device_details[i].device_name = new char[deviceNameLength];
        strcpy((char*)_instance.device_details[i].device_name, properties.deviceName);

        _instance.device_details[i].float_64_support = features.shaderFloat64;
        _instance.device_details[i].int_64_support = features.shaderInt64;
        _instance.device_details[i].int_16_support = features.shaderInt16;

        _instance.device_details[i].max_workgroup_size_x = properties.limits.maxComputeWorkGroupSize[0];
        _instance.device_details[i].max_workgroup_size_y = properties.limits.maxComputeWorkGroupSize[1];
        _instance.device_details[i].max_workgroup_size_z = properties.limits.maxComputeWorkGroupSize[2];

        _instance.device_details[i].max_workgroup_invocations = properties.limits.maxComputeWorkGroupInvocations;

        _instance.device_details[i].max_workgroup_count_x = properties.limits.maxComputeWorkGroupCount[0];
        _instance.device_details[i].max_workgroup_count_y = properties.limits.maxComputeWorkGroupCount[1];
        _instance.device_details[i].max_workgroup_count_z = properties.limits.maxComputeWorkGroupCount[2];

        _instance.device_details[i].max_descriptor_set_count = properties.limits.maxBoundDescriptorSets;

        _instance.device_details[i].max_push_constant_size = properties.limits.maxPushConstantsSize;
        _instance.device_details[i].max_storage_buffer_range = properties.limits.maxStorageBufferRange;
        _instance.device_details[i].max_uniform_buffer_range = properties.limits.maxUniformBufferRange;

        _instance.device_details[i].subgroup_size = subgroupProperties.subgroupSize;
        _instance.device_details[i].supported_stages = subgroupProperties.supportedStages;
        _instance.device_details[i].supported_operations = subgroupProperties.supportedOperations;
        _instance.device_details[i].quad_operations_in_all_stages = subgroupProperties.quadOperationsInAllStages;

        _instance.device_details[i].max_compute_shared_memory_size = properties.limits.maxComputeSharedMemorySize;

        //printf("Device %d: %s\n", i, _instance.devices[i].device_name);
        //printf("Atomics: %d\n", atomicFloatFeatures.shaderBufferFloat32Atomics);
        //printf("Atomics Add: %d\n", atomicFloatFeatures.shaderBufferFloat32AtomicAdd);
    }
}

struct PhysicalDeviceDetails* get_devices_extern(int* count) {
    *count = _instance.device_details.size();
    return _instance.device_details.data();
}